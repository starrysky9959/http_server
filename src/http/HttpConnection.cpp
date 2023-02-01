#include "HttpConnection.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <openssl/ssl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <glog/logging.h>

bool HttpConnection::isET_;
std::string HttpConnection::srcDir_;
std::atomic<int> HttpConnection::userCount_;

HttpConnection::HttpConnection() :
    fd_{-1}, addr_{0}, isClosed_{true} {
}

HttpConnection::~HttpConnection() {
    close();
}

void HttpConnection::init(int fd, const sockaddr_in &addr, bool openSSL, SSL *ssl) {
    assert(fd > 0);
    ++userCount_;
    this->addr_ = addr;
    this->fd_ = fd;
    this->openSSL_ = openSSL;
    this->ssl_ = ssl;
    readBuffer_.retrieveAll();
    writeBuffer_.retrieveAll();
    isClosed_ = false;

    LOG(INFO) << "Client[" << fd_ << getIP() << ":" << getPort() << ", userCount:" << (int)userCount_ << "]";
}

void HttpConnection::close() {
    response_.unmapFile();
    if (!isClosed_) {
        --userCount_;
        isClosed_ = true;
        ::close(fd_);
        LOG(INFO) << "Client[" << fd_ << getIP() << ":" << getPort() << ", userCount:" << (int)userCount_ << "]";
    }
}

sockaddr_in HttpConnection::getAddr() const {
    return addr_;
}

const char *HttpConnection::getIP() const {
    return inet_ntoa(addr_.sin_addr);
}

int HttpConnection::getPort() const {
    return addr_.sin_port;
}

int HttpConnection::getFd() const {
    return fd_;
}

ssize_t HttpConnection::read(int *saveErrno) {
    ssize_t len = -1;
    char tempBuffer[65536];
    if (openSSL_) {
        len = SSL_read(ssl_, (void *)(tempBuffer), sizeof(tempBuffer));
        if (len <= 0) {
            *saveErrno = errno;
            return len;
        }
        readBuffer_.append(tempBuffer, len);
        return len;
    }

    do {
        len = readBuffer_.readFd(fd_, saveErrno);
        if (len <= 0) {
            break;
        }
    } while (isET_);
    return len;
}

size_t HttpConnection::toWriteBytes() const {
    return iov_[0].iov_len + iov_[1].iov_len;
}

bool HttpConnection::isKeepAlive() const {
    return request_.isKeepAlive();
}

ssize_t HttpConnection::write(int *saveErrno) {
    ssize_t len = -1;
    if (openSSL_) {
        do {
            // LOG(INFO) << "to write byte:" << toWriteBytes();
            len = SSL_write(ssl_, iov_[0].iov_base, (int)iov_[0].iov_len);
            if (len <= 0) {
                *saveErrno = errno;
                break;
            }
            iov_[0].iov_base = static_cast<uint8_t *>(iov_[0].iov_base) + len;
            iov_[0].iov_len -= len;
            writeBuffer_.retrieve(len);

        } while (iov_[0].iov_len);
        if (iov_[0].iov_len) {
            writeBuffer_.retrieveAll();
            iov_[0].iov_len = 0;
        }

        do {
            len = SSL_write(ssl_, iov_[1].iov_base, (int)iov_[1].iov_len);
            if (len <= 0) {
                *saveErrno = errno;
                break;
            }
            iov_[1].iov_base = static_cast<uint8_t *>(iov_[1].iov_base) + len;
            iov_[1].iov_len -= len;
            // writeBuffer_.retrieve(len);
        } while (iov_[1].iov_len > 0);
        return len;
    }

    do {
        len = writev(fd_, iov_, iovCnt_);
        if (len <= 0) {
            *saveErrno = errno;
            break;
        }

        // transport finish
        if (iov_[0].iov_len + iov_[1].iov_len == 0) {
            break;
        } else if (static_cast<size_t>(len) > iov_[0].iov_len) {
            iov_[1].iov_base = static_cast<uint8_t *>(iov_[1].iov_base) + (len - iov_[0].iov_len);
            iov_[1].iov_len -= (len - iov_[0].iov_len);
            if (iov_[0].iov_len) {
                writeBuffer_.retrieveAll();
                iov_[0].iov_len = 0;
            }
        } else {
            iov_[0].iov_base = static_cast<uint8_t *>(iov_[0].iov_base) + len;
            iov_[0].iov_len -= len;
            writeBuffer_.retrieve(len);
        }
    } while (isET_ || toWriteBytes() > 10240);
    return len;
}

bool HttpConnection::process() {
    request_.init();
    if (readBuffer_.readableBytes() <= 0) {
        return false;
    } else if (request_.parse(readBuffer_)) {
        response_.init(srcDir_, request_, request_.isKeepAlive(), 200);
    } else {
        response_.init(srcDir_, request_, false, 400);
    }

    response_.makeResponse(writeBuffer_);

    // response header
    iov_[0].iov_base = const_cast<char *>(writeBuffer_.peek());
    iov_[0].iov_len = writeBuffer_.readableBytes();
    iovCnt_ = 1;

    // file
    if (response_.getFileLength() > 0 && response_.getFile()) {
        iov_[1].iov_base = response_.getFile();
        iov_[1].iov_len = response_.getFileLength();
        iovCnt_ = 2;
    }

    return true;
}