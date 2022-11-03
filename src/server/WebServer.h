#pragma once

#include "Epoller.h"
#include <cstdint>
#include <netinet/in.h>
#include "../http/HttpConnection.h"
#include "../pool/ThreadPool.h"
#include "../log/Log.h"

class WebServer {
public:
    WebServer(int port, int trigMode, int timeout,bool optLinger,  int threadNum=2,bool openLog=true, int logLevel=0, int logQueSize=1024);
    ~WebServer();
    void start();

private:
    bool initSocket();
    void initEventMode(int trigMode);
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

    int port_;
    bool openLinger_;
    int trigMode_;
    int timeout_;
    int connectionPoolNum_;
    int threadNum_;

    bool isClosed_;
    int listenFd_; // 监听socket file descriptor
    char *srcDir_;

    uint32_t listenEvent_;
    uint32_t connectionEvent_;

    std::unique_ptr<ThreadPool> threadPool_;
    std::unique_ptr<Epoller> epoller_;
    std::unordered_map<int, HttpConnection> users_; // 已连接的socket file descriptor

    constexpr static int MAX_FD = 65536;
};
