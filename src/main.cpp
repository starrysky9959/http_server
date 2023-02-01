/*
 * @Author: starrysky9959 965105951@qq.com
 * @Date: 2022-10-16 22:30:41
 * @LastEditors: starrysky9959 starrysky9651@outlook.com
 * @LastEditTime: 2023-02-02 00:31:20
 * @Description: 
 */
#include <cstdlib>
#include <iostream>
#include <queue>
#include <string>
#include <sys/types.h>
#include <thread>
#include <vector>
#include "server/WebServer.h"
#include <glog/logging.h>

int main(int argc, char *argv[]) {
    // config glog
    google::InitGoogleLogging(argv[0]);
    google::SetLogDestination(google::FATAL, "./log/");
    // FLAGS_logbufsecs = 0;
    google::InstallFailureSignalHandler();

    // 80 and 443 port need run with sudo
    WebServer httpServer(80, false, 3, 60000, false, 8);  // port, openSSL, ET, timeout, linger, threadNum
    WebServer httpsServer(443, true, 3, 60000, false, 8); // port, openSSL, ET, timeout, linger, threadNum
    auto thread1 = std::thread([&] {
        LOG(INFO) << "HTTP thread is starting.";
        httpServer.start();
    });
    auto thread2 = std::thread([&] {
        LOG(INFO) << "HTTPS thread is starting.";
        httpsServer.start();
    });
    thread1.join();
    thread2.join();
}