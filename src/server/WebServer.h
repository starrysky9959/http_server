/*
 * @Author: starrysky9959 965105951@qq.com
 * @Date: 2022-10-27 23:53:20
 * @LastEditors: starrysky9959 starrysky9651@outlook.com
 * @LastEditTime: 2023-01-30 15:35:32
 * @Description:  
 */
#pragma once

#include "Epoller.h"
#include <cstdint>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "../http/HttpConnection.h"
#include "../pool/ThreadPool.h"

class WebServer {
public:
    WebServer(int port,  bool openSSL,int trigMode, int timeout, bool optLinger, int threadNum = 2);
    ~WebServer();
    void start();

private:
    bool initSocket();
    void initEventMode(int trigMode);
    bool initSSL(const char* cert="./keys/cert.pem", const char* key="./keys/privkey.pem", const char* passwd="123456");
    void addClient(int fd, sockaddr_in addr);

    void dealListen();
    void dealRead(HttpConnection *client);
    void dealWrite(HttpConnection *client);

    void closeConnection(HttpConnection *client);
    void extendTime(HttpConnection *client);
    void onRead(HttpConnection *client);
    void onWrite(HttpConnection *client);
    void onProcess(HttpConnection *client);

    void sendError(int fd, const char *info);

    static int setFdNonBlock(int fd);

    bool openSSL_;
    int port_;
    bool openLinger_;
    int trigMode_;
    int timeout_;
    int connectionPoolNum_;
    int threadNum_;

    bool isClosed_;
    int listenFd_; // 监听socket file descriptor
    std::string srcDir_;

    SSL_CTX * ctx_ ; 
    SSL * ssl_;

    uint32_t listenEvent_;
    uint32_t connectionEvent_;

    std::unique_ptr<ThreadPool> threadPool_;
    std::unique_ptr<Epoller> epoller_;
    std::unordered_map<int, HttpConnection> users_; // 已连接的socket file descriptor

    constexpr static int MAX_FD = 65536;
};
