#include "HttpConnection.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
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

void HttpConnection::init(int fd, const sockaddr_in &addr) {
    assert(fd > 0);
    ++userCount_;
    this->addr_ = addr;
    this->fd_ = fd;
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
        response_.Init(srcDir_, request_.getPath(), request_.isKeepAlive(), 200);
    } else {
        response_.Init(srcDir_, request_.getPath(), false, 400);
    }

    std::cout<<"make response "<< std::endl;
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
    LOG_DEBUG("filesize:%d, %d  to %d", response_.getFileLength() , iovCnt_, toWriteBytes());
    return true;
}