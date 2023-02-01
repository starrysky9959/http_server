/*
 * @Author: starrysky9959 965105951@qq.com
 * @Date: 2022-10-27 23:53:28
 * @LastEditors: starrysky9959 starrysky9651@outlook.com
 * @LastEditTime: 2023-02-02 00:18:15
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
#include <openssl/rsa.h>
#include <ostream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <glog/logging.h>
#include <filesystem>

WebServer::WebServer(int port, bool openSSL, int trigMode, int timeout, bool optLinger, int threadNum) :
    port_{port}, trigMode_{trigMode}, openSSL_{openSSL}, timeout_{timeout}, openLinger_(optLinger), isClosed_{false}, threadPool_{std::make_unique<ThreadPool>(threadNum)}, epoller_{std::make_unique<Epoller>()} {
    // get current path

    srcDir_ = std::filesystem::current_path();
    LOG(INFO) << "current path: " << srcDir_;
    srcDir_ += "/resources";
    LOG(INFO) << "resources path: " << srcDir_;

    HttpConnection::srcDir_ = srcDir_;
    HttpConnection::userCount_ = 0;

    initEventMode(trigMode);
    if (!initSocket()) {
        isClosed_ = true;
    }

    if (openSSL_) {
        assert(initSSL());
        LOG(INFO) << "Enable OpenSSL";
    }

    LOG(INFO) << "Start success";
}

WebServer::~WebServer() {
    close(listenFd_);
    isClosed_ = true;
    // free(srcDir_);
    if (openSSL_) {
        SSL_CTX_free(ctx_);
    }
}

/**
 * @description: reference link: https://blog.csdn.net/qq_42370809/article/details/126352996
 */
bool WebServer::initSSL(const char *cert, const char *key, const char *passwd) {
    // init
    SSLeay_add_ssl_algorithms();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    ERR_load_BIO_strings();

    // 我们使用SSL V3,V2
    ctx_ = SSL_CTX_new(SSLv23_method());
    assert(ctx_ != NULL);

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

    // check whether the port is legal
    // if (port_ > 65535 || port_ < 1024) {
    //     std::cout<<"port number illegal"<<std::endl;
    //     return false;
    // }

    // create listen socket file descriptor
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) {
        LOG(INFO) << "Create socket error!" << port_;
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
        LOG(ERROR) << "Init linger error! port=" << port_;
        return false;
    }

    int optVal = 1;
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void *)(&optVal), sizeof(optVal));
    if (ret < 0) {
        LOG(ERROR) << "set socket setsockopt error !";
        close(listenFd_);
        return false;
    }

    // bind address to this socket
    struct sockaddr_in addr;
    // address family: AF_INET menas IPv4 Internet protocol
    addr.sin_family = AF_INET;                
    // 将主机无符号长整型数转换成网络字节顺序. INADDR_ANY就是指定地址0.0.0.0的地址, 这个地址事实上表示不确定地址, 或"所有地址", "任意地址". 一般来说，在各个系统中均定义成为0值.
    addr.sin_addr.s_addr = htonl(INADDR_ANY); 
    // 将一个无符号短整型数值转换为网络字节序, 即大端模式
    addr.sin_port = htons(port_);             
    ret = bind(listenFd_, (struct sockaddr *)(&addr), sizeof(addr));
    if (ret < 0) {
        LOG(ERROR) << "Bind Port:" << port_ << " error!";
        close(listenFd_);
        return false;
    }

    // listen this socket
    ret = listen(listenFd_, 6);
    if (ret < 0) {
        LOG(ERROR) << "Listen port:" << port_ << "error!";
        close(listenFd_);
        return false;
    }

    // register new event
    ret = epoller_->addFd(listenFd_, listenEvent_ | EPOLLIN);
    if (ret == 0) {
        LOG(ERROR) << "Add listen error!";
        close(listenFd_);
        return false;
    }

    setFdNonBlock(listenFd_);
    LOG(INFO) << "Server port:" << port_;
    return true;
}

int WebServer::setFdNonBlock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}

void WebServer::initEventMode(int trigMode) {
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

    users_[fd].init(fd, addr, openSSL_, ssl_);
    epoller_->addFd(fd, connectionEvent_ | EPOLLIN);
    setFdNonBlock(fd);
    LOG(INFO) << "Client[" << users_[fd].getFd() << "] in!";
}

void WebServer::start() {
    if (!isClosed_) {
        LOG(INFO) << "========== Server start ==========";
    }

    while (!isClosed_) {
        // epoll wait timeout == -1, it will block if there is no event happens
        int eventCnt = epoller_->wait(-1);

        LOG(INFO) << "event cnt: " << eventCnt;
        for (int i = 0; i < eventCnt; ++i) {
            LOG(INFO) << "have event now";
            // handle event according to it's type
            int fd = epoller_->getEventFd(i);
            uint32_t events = epoller_->getEvents(i);
            if (fd == listenFd_) {
                LOG(INFO) << "Deal Listen";
                dealListen();
            } else if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                LOG(INFO) << "Close connection";
                assert(users_.count(fd) > 0);
                closeConnection(&users_[fd]);
            } else if (events & EPOLLIN) {
                LOG(INFO) << "Deal read";
                assert(users_.count(fd) > 0);
                dealRead(&users_[fd]);
            } else if (events & EPOLLOUT) {
                LOG(INFO) << "Deal write";
                assert(users_.count(fd) > 0);
                dealWrite(&users_[fd]);
            } else {
                LOG(ERROR) << "Unexpected event";
            }
        }
    }
}

void WebServer::dealListen() {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do {
        // receive connection request and build a connection file descriptor
        int fd = accept(listenFd_, (struct sockaddr *)(&addr), &len);
        LOG(INFO) << "accept: " << fd;

        if (fd < 0) {
            return;
        } else if (HttpConnection::userCount_ >= MAX_FD) {
            LOG(INFO) << "Clients is full!";
            sendError(fd, "Server busy! Too much connections!");
            return;
        }

        if (openSSL_) {
            // create ssl socket
            ssl_ = SSL_new(ctx_);
            if (ssl_ == NULL) {
                LOG(ERROR) << "SSL new wrong";
                return;
            }
            SSL_set_accept_state(ssl_);
            // bind stream socket in read/write mode
            SSL_set_fd(ssl_, fd);

            // SSL handshake
            auto ret = SSL_accept(ssl_);
            if (ret != 1) {
                LOG(ERROR) << SSL_state_string_long(ssl_);
                LOG(ERROR) << "ret = " << ret << " . ssl get error: " << SSL_get_error(ssl_, ret);
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
       LOG(INFO)<<"send error to client["<<fd<<"] error!";
    }
    close(fd);
}

void WebServer::closeConnection(HttpConnection *client) {
    assert(client);
    epoller_->delFd(client->getFd());

    // close
    if (openSSL_ && ssl_ != NULL) {
        // close SSL socket
        SSL_shutdown(ssl_); 
        // release SSL socket
        SSL_free(ssl_);     
    }
    client->close();
}
void WebServer::extendTime(HttpConnection *client) {
    assert(client);
}

void WebServer::onRead(HttpConnection *client) {
    assert(client);

    int readErrno = 0;
    auto ret = client->read(&readErrno);
    if (ret <= 0 && readErrno != EAGAIN) {
        closeConnection(client);
        return;
    }

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