#include "HttpConnection.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <openssl/ssl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include "../log/Log.h"

bool HttpConnection::isET_;
const char *HttpConnection::srcDir_;
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

    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", fd_, getIP(), getPort(), (int)userCount_);
}

void HttpConnection::close() {
    response_.unmapFile();
    if (!isClosed_) {
        --userCount_;
        isClosed_ = true;
        ::close(fd_);
        LOG_INFO("Client[%d](%s:%d) quit, userCount:%d", fd_, getIP(), getPort(), (int)userCount_);
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
            int ret;
            std::cout << SSL_get_error(ssl_, ret) << std::endl;
            return len;
        }
        readBuffer_.append(tempBuffer, len);
        std::cout << "ssl read: " << tempBuffer << std::endl;
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
    /*
    if (openSSL_) {
        len = SSL_write(ssl_, iov_[0].iov_base, (int)iov_[0].iov_len);
        if (len <= 0) {
            *saveErrno = errno;
            int ret;
            std::cout << "ssl get error: "<<SSL_get_error(ssl_, ret) << std::endl;
            return len;
        }
        len = SSL_write(ssl_, iov_[1].iov_base, (int)iov_[1].iov_len);
        if (len <= 0) {
            *saveErrno = errno;
            int ret;
            std::cout << "ssl get error: "<<SSL_get_error(ssl_, ret) << std::endl;
            return len;
        }
        return len;
    }
    */

    if (openSSL_) {
        do {
            std::cout << "to write byte:" << toWriteBytes() << std::endl;
            len = SSL_write(ssl_, iov_[0].iov_base, (int)iov_[0].iov_len);
            std::cout << "len:" << len << std::endl;
            if (len <= 0) {
                *saveErrno = errno;
                std::cout<<"errno: "<<errno<<std::endl;
                int ret;
                std::cout << "ssl get error: " << SSL_get_error(ssl_, ret) << std::endl;
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
        std::cout << "write iov[0] finish" << std::endl;
        do {
            std::cout << "to write byte:" << toWriteBytes() << std::endl;
            len = SSL_write(ssl_, iov_[1].iov_base, (int)iov_[1].iov_len);
            std::cout << "len:" << len << std::endl;
            if (len <= 0) {
                *saveErrno = errno;
                std::cout<<"errno: "<<errno<<std::endl;
                int ret;
                std::cout << "ssl get error: " << SSL_get_error(ssl_, ret) << std::endl;
                break;
            }
            iov_[1].iov_base = static_cast<uint8_t *>(iov_[1].iov_base) + len;
            iov_[1].iov_len -= len;
            // writeBuffer_.retrieve(len);
        } while (iov_[1].iov_len>0);
        std::cout << "write iov[1] finish" << std::endl;
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
        response_.init(srcDir_, request_.getPath(), request_.getHeader(), request_.isKeepAlive(), 200);
    } else {
        response_.init(srcDir_, request_.getPath(), request_.getHeader(), false, 400);
    }

    std::cout << "make response " << std::endl;
    response_.makeResponse(writeBuffer_, !openSSL_);

    // response header
    iov_[0].iov_base = const_cast<char *>(writeBuffer_.peek());
    iov_[0].iov_len = writeBuffer_.readableBytes();
    iovCnt_ = 1;

    // file
    if (response_.getFileLength() > 0 && response_.getFile()) {
        iov_[1].iov_base = response_.getFile();
        iov_[1].iov_len = response_.getFileLength();
        iovCnt_ = 2;

        std::cout << "begin:" << response_.begin_ << std::endl;
        std::cout << "end:" << response_.end_ << std::endl;
        if (response_.begin_ != -1 && response_.end_ != -1) {
            std::cout << "this 1" << std::endl;
            iov_[1].iov_base = static_cast<uint8_t *>(iov_[1].iov_base) + response_.begin_;
            iov_[1].iov_len = (response_.end_ - response_.begin_ + 1);
        } else if (response_.begin_ != -1 && response_.end_ == -1) {
            std::cout << "this 2" << std::endl;
            iov_[1].iov_base = static_cast<uint8_t *>(iov_[1].iov_base) + response_.begin_;
            iov_[1].iov_len -= (response_.begin_);
        }
    }
    // LOG_DEBUG("filesize:%d, %d  to %d", response_.getFileLength(), iovCnt_, toWriteBytes());
    return true;
}