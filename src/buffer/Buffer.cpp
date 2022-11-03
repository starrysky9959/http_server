/*
 * @Author:starrysky
 * @Date:2022-10-19
*/
#include "Buffer.h"
#include <bits/types/struct_iovec.h>
#include <sys/uio.h>
#include <string.h>
#include <cassert>
#include <cstddef>
#include <sys/types.h>

Buffer::Buffer(int size) :
    buffer(size), readPos(0), writePos(0) {
}

size_t Buffer::readableBytes() const {
    return writePos - readPos;
}
size_t Buffer::writeableBytes() const {
    return buffer.size() - writePos;
}

size_t Buffer::prependableBytes() const {
    return readPos;
}

const char *Buffer::peek() const {
    return beginPtr() + readPos;
}

void Buffer::retrieve(size_t len) {
    assert(len <= readableBytes());
    readPos += len;
}

void Buffer::retrieveUntil(const char *end) {
    assert(peek() <= end);
    retrieve(end - peek());
}

std::string Buffer::retrieveUntilToString(const char *end) {
    assert(peek() <= end);
    std::string str(peek(), end);
    retrieve(end - peek());
    return str;
}

void Buffer::retrieveAll() {
    // bzero()不是ANSI C函数, 其起源于早期的Berkeley网络编程代码, 但是几乎所有支持套接字API的厂商都提供该函数
    // memset()为ANSI C函数, 更常规、用途更广
    bzero(beginPtr(), buffer.size());
    readPos = 0;
    writePos = 0;
}

std::string Buffer::retrieveAllToString() {
    std::string str(peek(), readableBytes());
    retrieveAll();
    return str;
}

void Buffer::hasWritten(size_t len) {
    writePos += len;
}

char *Buffer::beginWrite() {
    return beginPtr() + writePos;
}

const char *Buffer::beginWriteConst() const {
    return beginPtr() + writePos;
}

Buffer &Buffer::append(const std::string &str) {
    append(str.data(), str.length());
    return *this;
}
Buffer &Buffer::append(const char *str, size_t len) {
    assert(str);
    ensureWriteable(len);
    std::copy(str, str + len, beginWrite());
    hasWritten(len);
    return *this;
}

Buffer &Buffer::append(const void *data, size_t len) {
    assert(data);
    return append(static_cast<const char *>(data), len);
}

Buffer &Buffer::append(const Buffer &buffer_) {
    return append(buffer_.peek(), buffer_.readableBytes());
}

ssize_t Buffer::readFd(int fd, int *saveErrno) {
    char tempBuffer[65535];
    struct iovec iov[2];
    const size_t writeable = writeableBytes();

    // 分散读scatter read
    // 确保数据全部读完
    iov[0].iov_base = beginWrite();
    iov[0].iov_len = writeable;
    iov[1].iov_base = tempBuffer;
    iov[1].iov_len = sizeof(tempBuffer);

    // total byte size
    const ssize_t len = readv(fd, iov, 2);
    if (len < 0) {
        *saveErrno = errno;
    } else if (static_cast<size_t>(len) <= writeable) {
        writePos += len;
    } else {
        writePos = buffer.size();
        append(tempBuffer, len - writeable);
    }
    return len;
}

ssize_t Buffer::writeFd(int fd, int *saveErrno) {
    ssize_t len = write(fd, peek(), readableBytes());
    if (len < 0) {
        *saveErrno = errno;
        return len;
    }
    readPos += len;
    return len;
}

char *Buffer::beginPtr() {
    return &(*buffer.begin());
}

const char *Buffer::beginPtr() const {
    return &(*buffer.begin());
}

void Buffer::makeSpace(size_t len) {
    if (writeableBytes() + prependableBytes() < len) {
        buffer.resize(writePos + len + 1);
    } else { // if there is no need to resize
        size_t readable = readableBytes();
        // cover the part have been read
        std::copy(beginPtr() + readPos, beginPtr() + writePos, beginPtr());
        readPos = 0;
        writePos = readPos + readable;
        assert(readable == readableBytes());
    }
}

void Buffer::ensureWriteable(size_t len) {
    if (writeableBytes() < len) {
        makeSpace(len);
    }
    assert(writeableBytes() >= len);
}