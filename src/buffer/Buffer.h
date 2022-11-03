/*
 * @Author:starrysky
 * @Date:2022-10-19
*/
#pragma once

#include <atomic>
#include <cstddef>
#include <string>
#include <unistd.h>
#include <vector>
class Buffer {
public:
    Buffer(int size = 1024);
    ~Buffer() = default;

    // remain to read
    size_t readableBytes() const;
    // remain to write
    size_t writeableBytes() const;
    // have been read
    size_t prependableBytes() const;

    const char *peek() const;
    char *beginWrite();
    const char *beginWriteConst() const;

    // retrieve operations
    void retrieve(size_t len);
    void retrieveUntil(const char *end);
    std::string retrieveUntilToString(const char *end);
    void retrieveAll();
    std::string retrieveAllToString();

    // write operations
    Buffer& append(const std::string &str);
    Buffer& append(const char *str, size_t len);
    Buffer& append(const void *data, size_t len);
    Buffer& append(const Buffer &buffer_);
    void hasWritten(size_t len);

    ssize_t readFd(int fd, int * saveErrno);
    ssize_t writeFd(int fd, int * saveErrno);

private:
    char *beginPtr();
    const char *beginPtr() const;
    void ensureWriteable(size_t len);
    void makeSpace(size_t len);

    std::vector<char> buffer;
    std::atomic<std::size_t> readPos;
    std::atomic<std::size_t> writePos;
};
