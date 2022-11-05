/*
 * @Author: starrysky9959 965105951@qq.com
 * @Date: 2022-10-27 23:53:28
 * @LastEditors: starrysky9959 965105951@qq.com
 * @LastEditTime: 2022-11-05 14:18:41
 * @Description:  
 */

#include "WebServer.h"
#include "Epoller.h"
#include <asm-generic/errno-base.h>
#include <asm-generic/socket.h>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <iostream>
#include <netinet/in.h>
#include <ostream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

WebServer::WebServer(int port, bool openSSL, int trigMode, int timeout, bool optLinger, int threadNum, bool openLog, int logLevel, int logQueSize) :
    port_{port}, trigMode_{trigMode}, openSSL_{openSSL}, timeout_{timeout}, openLinger_(optLinger), isClosed_{false}, threadPool_{std::make_unique<ThreadPool>(threadNum)}, epoller_{std::make_unique<Epoller>()} {
    // timer_(new HeapTimer()), threadpool_(new ThreadPool(threadNum)), epoller_(new Epoller())
    // get current path
    srcDir_ = getcwd(nullptr, 256);
    assert(srcDir_);
    strncat(srcDir_, "/resources", 16);

    HttpConnection::srcDir_ = srcDir_;
    HttpConnection::userCount_ = 0;
    std::cout << srcDir_ << std::endl;
    initEventMode(trigMode);
    if (!initSocket()) {
        isClosed_ = true;
    }

    if (openSSL_) {
        assert(initSSL());
        std::cout << "Enable OpenSSL" << std::endl;
        LOG_INFO("Enable OpenSSL");
    }

    // std::cout << openLog << isClosed_ << std::endl;

    if (openLog) {
        Log::Instance()->init(logLevel, "./log", ".log", logQueSize);
        if (isClosed_) {
            LOG_ERROR("========== Server init error!==========");
        } else {
            LOG_INFO("========== Server init ==========");
            LOG_INFO("Port:%d, OpenLinger: %s", port_, optLinger ? "true" : "false");
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                     (listenEvent_ & EPOLLET ? "ET" : "LT"),
                     (connectionEvent_ & EPOLLET ? "ET" : "LT"));
            LOG_INFO("LogSys level: %d", logLevel);
            LOG_INFO("srcDir: %s", HttpConnection::srcDir_);
            LOG_INFO("ThreadPool num: %d", threadNum);
        }
    }
}

WebServer::~WebServer() {
    close(listenFd_);
    isClosed_ = true;
    free(srcDir_);
    if (openSSL_) {
        SSL_CTX_free(ctx_);
    }
}

bool WebServer::initSSL(const char *cert, const char *key, const char *passwd) {
    // init
    SSLeay_add_ssl_algorithms();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    ERR_load_BIO_strings();

    // 我们使用SSL V3,V2
    ctx_ = SSL_CTX_new(SSLv23_method());
    assert(ctx_ != nullptr);

    // 要求校验对方证书，这里建议使用SSL_VERIFY_FAIL_IF_NO_PEER_CERT，详见https://blog.csdn.net/u013919153/article/details/78616737
    //对于服务器端来说如果使用的是SSL_VERIFY_PEER且服务器端没有考虑对方没交证书的情况，会出现只能访问一次，第二次访问就失败的情况。
    SSL_CTX_set_verify(ctx_, SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);

    // 加载CA的证书
    assert(SSL_CTX_load_verify_locations(ctx_, cert, NULL));

    // 加载自己的证书
    assert(SSL_CTX_use_certificate_chain_file(ctx_, cert) > 0);

    // 加载自己的私钥
    SSL_CTX_set_default_passwd_cb_userdata(ctx_, (void *)passwd);
    assert(SSL_CTX_use_PrivateKey_file(ctx_, key, SSL_FILETYPE_PEM) > 0);

    // 判定私钥是否正确
    assert(SSL_CTX_check_private_key(ctx_));

    return true;
}

/**
 * @description: reference link: https://www.jianshu.com/p/3b233facd6bb
 */
bool WebServer::initSocket() {
    int ret;

    //check whether the port is legal
    // if (port_ > 65535 || port_ < 1024) {
    //     std::cout<<"port number illegal"<<std::endl;
    //     return false;
    // }

    // create listen socket file descriptor
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) {
        LOG_ERROR("Create socket error!", port_);
        std::cout << "Create socket error!" << std::endl;
        return false;
    }

    // about function setsocket: https://www.cnblogs.com/cthon/p/9270778.html
    struct linger optLinger = {0};
    if (openLinger_) {
        // 优雅关闭: 直到所剩数据发送完毕或超时, close或shutdown才会返回
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if (ret < 0) {
        close(listenFd_);
        LOG_ERROR("Init linger error!", port_);
        std::cout << "Init linger error!" << std::endl;
        return false;
    }

    int optVal = 1;
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void *)(&optVal), sizeof(optVal));
    if (ret < 0) {
        LOG_ERROR("set socket setsockopt error !");
        close(listenFd_);
        std::cout << "set socket setsockopt error !" << std::endl;
        return false;
    }

    // bind address to this socket
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;                // address family: AF_INET menas IPv4 Internet protocol
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // 将主机无符号长整型数转换成网络字节顺序. INADDR_ANY就是指定地址0.0.0.0的地址, 这个地址事实上表示不确定地址, 或"所有地址", "任意地址". 一般来说，在各个系统中均定义成为0值.
    addr.sin_port = htons(port_);             // 将一个无符号短整型数值转换为网络字节序, 即大端模式
    ret = bind(listenFd_, (struct sockaddr *)(&addr), sizeof(addr));
    if (ret < 0) {
        LOG_ERROR("Bind Port:%d error!", port_);
        close(listenFd_);
        std::cout << "Bind Port error!" << std::endl;
        return false;
    }

    // listen this socket
    ret = listen(listenFd_, 6);
    if (ret < 0) {
        LOG_ERROR("Listen port:%d error!", port_);
        close(listenFd_);
        std::cout << "Listen port: error!" << std::endl;
        return false;
    }

    // register new event
    ret = epoller_->addFd(listenFd_, listenEvent_ | EPOLLIN);
    if (ret == 0) {
        LOG_ERROR("Add listen error!");
        close(listenFd_);
        std::cout << "Add listen error!" << std::endl;
        return false;
    }

    setFdNonBlock(listenFd_);
    LOG_INFO("Server port:%d", port_);
    return true;
}

int WebServer::setFdNonBlock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}

void WebServer::initEventMode(int trigMode) {
    // ??? dont't understand
    // https://mp.weixin.qq.com/s/BfnNl-3jc_x5WPrWEJGdzQ
    listenEvent_ = EPOLLRDHUP;
    connectionEvent_ = EPOLLONESHOT | EPOLLRDHUP;

    switch (trigMode) {
    case 0:
        break;
    case 1:
        connectionEvent_ |= EPOLLET;
        break;
    case 2:
        listenEvent_ |= EPOLLET;
        break;
    case 3:
        // connectionEvent_ |= EPOLLET;
        // // listenEvent_ |= EPOLLET;
        // break;
    default:
        connectionEvent_ |= EPOLLET;
        listenEvent_ |= EPOLLET;
        break;
    }
    HttpConnection::isET_ = (connectionEvent_ & EPOLLET);
}

void WebServer::addClient(int fd, sockaddr_in addr) {
    assert(fd > 0);
    std::cout << "add client" << std::endl;

    users_[fd].init(fd, addr, openSSL_, ssl_);
    epoller_->addFd(fd, connectionEvent_ | EPOLLIN);
    setFdNonBlock(fd);
    LOG_INFO("Client[%d] in!", users_[fd].getFd());
}

void WebServer::start() {
    int timeMs = -1; // epoll wait timeout == -1, it will block if there is no event happens
    if (!isClosed_) {
        std::cout << "========== Server start ==========" << std::endl;
        // LOG_INFO("========== Server start ==========");
    }

    while (!isClosed_) {
        if (timeMs > 0) {
        }

        int eventCnt = epoller_->wait(-1);
        std::cout << "event: " << eventCnt << std::endl;
        LOG_DEBUG("event cnt %d", eventCnt);
        for (int i = 0; i < eventCnt; ++i) {
            std::cout << "have event" << std::endl;
            // handle event according to it's type
            int fd = epoller_->getEventFd(i);
            uint32_t events = epoller_->getEvents(i);
            if (fd == listenFd_) {
                dealListen();
                LOG_DEBUG("dealListen");
            } else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                std::cout << "close begin" << std::endl;
                assert(users_.count(fd) > 0);
                closeConnection(&users_[fd]);
                std::cout << "close end" << std::endl;
            } else if (events & EPOLLIN) {
                std::cout << "dealread begin" << std::endl;
                assert(users_.count(fd) > 0);
                dealRead(&users_[fd]);
                std::cout << "dealread end" << std::endl;
            } else if (events & EPOLLOUT) {
                std::cout << "dealwrite begin" << std::endl;
                assert(users_.count(fd) > 0);
                dealWrite(&users_[fd]);
                std::cout << "dealwrite end" << std::endl;
            } else {
                LOG_ERROR("Unexpected event");
                std::cout << "Unexpected event" << std::endl;
            }
        }
    }
}

void WebServer::dealListen() {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do {
        // receive connection request and build a connectioned file descriptor
        int fd = accept(listenFd_, (struct sockaddr *)(&addr), &len);
        std::cout << "accept: " << fd << std::endl;

        if (fd < 0) {
            return;
        } else if (HttpConnection::userCount_ >= MAX_FD) {
            LOG_WARN("Clients is full!");
            sendError(fd, "Server busy! Too much connections!");

            return;
        }

        if (openSSL_) {
            // create ssl socket
            ssl_ = SSL_new(ctx_);
            if (ssl_ == NULL) {
                LOG_ERROR("SSL new wrong");
                std::cout << "SSL new wrong" << std::endl;
                return;
            }
            SSL_set_accept_state(ssl_);
            // bind stream socket in read/write mode
            SSL_set_fd(ssl_, fd);

            // 完成SSL握手
            auto ret = SSL_accept(ssl_);
            if (ret != 1) {
                LOG_ERROR("%s\n", SSL_state_string_long(ssl_));
                std::cout << SSL_state_string_long(ssl_) << std::endl;
                LOG_ERROR("ret = %d, ssl get error %d\n", ret, SSL_get_error(ssl_, ret));
                std::cout << "ret = " << ret << " ssl get error " << SSL_get_error(ssl_, ret) << std::endl;
                return;
            }
        }
        addClient(fd, addr);
    } while (listenEvent_ & EPOLLET);
}

void WebServer::dealRead(HttpConnection *client) {
    assert(client);
    threadPool_->addTask(std::bind(&WebServer::onRead, this, client));
}

void WebServer::dealWrite(HttpConnection *client) {
    assert(client);
    threadPool_->addTask(std::bind(&WebServer::onWrite, this, client));
}

void WebServer::sendError(int fd, const char *info) {
    assert(fd > 0);
    auto ret = send(fd, info, strlen(info), 0);
    if (ret < 0) {
        //
    }
    close(fd);
}

void WebServer::closeConnection(HttpConnection *client) {
    assert(client);
    epoller_->delFd(client->getFd());
    client->close();

    // 关闭
    if (openSSL_ && ssl_ != NULL) {
        SSL_shutdown(ssl_); // 关闭SSL套接字
        SSL_free(ssl_);     // 释放SSL套接字
        //
    }
}
void WebServer::extendTime(HttpConnection *client) {
    assert(client);
}

void WebServer::onRead(HttpConnection *client) {
    assert(client);
    LOG_DEBUG("begin on read");
    std::cout << "begin on read" << std::endl;

    int readErrno = 0;
    auto ret = client->read(&readErrno);
    if (ret <= 0 && readErrno != EAGAIN) {
        closeConnection(client);
        return;
    }

    std::cout << "end on read" << std::endl;
    onProcess(client);
}

void WebServer::onWrite(HttpConnection *client) {
    assert(client);
    int writeErrno = 0;
    auto ret = client->write(&writeErrno);
    if (client->toWriteBytes() == 0) {
        // transport completed
        if (client->isKeepAlive()) {
            onProcess(client);
            return;
        }
    } else if ((ret < 0) && (writeErrno == EAGAIN)) {
        // transport continue
        epoller_->modFd(client->getFd(), connectionEvent_ | EPOLLOUT);
        return;
    }

    closeConnection(client);
}

void WebServer::onProcess(HttpConnection *client) {
    if (client->process()) {
        // enable write
        epoller_->modFd(client->getFd(), connectionEvent_ | EPOLLOUT);
    } else {
        // enable read or close
        epoller_->modFd(client->getFd(), connectionEvent_ | EPOLLIN);
    }
}