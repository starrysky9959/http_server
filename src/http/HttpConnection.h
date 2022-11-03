#pragma once

#include "HttpRequest.h"
#include "HttpResponse.h"
#include <arpa/inet.h> // sockaddr_in
#include <bits/types/struct_iovec.h>
#include <netinet/in.h> // sockaddr_in
#include <unistd.h>

class HttpConnection {
public:
    HttpConnection();
    ~HttpConnection();

    void init(int fd, const sockaddr_in &addr);
    void close();

    bool process();

    ssize_t read(int *saveErrno);
    ssize_t write(int *saveErrno);

    size_t toWriteBytes() const;
    sockaddr_in getAddr() const;
    const char *getIP() const;
    int getPort() const;
    int getFd() const;
    bool isKeepAlive() const;

    static bool isET_;
    static const char *srcDir_;
    static std::atomic<int> userCount_;
private:
    int fd_;
    struct sockaddr_in addr_;
    bool isClosed_;

    // 定义了一个向量元素, 通常这个结构用作一个多元素的数组
    struct iovec iov_[2];
    int iovCnt_;

    Buffer readBuffer_;
    Buffer writeBuffer_;

    HttpRequest request_;
    HttpResponse response_;
};
