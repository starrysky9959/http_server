/*
 * @Author: starrysky9959 965105951@qq.com
 * @Date: 2022-10-16 22:30:41
 * @LastEditors: starrysky9959 starrysky9651@outlook.com
 * @LastEditTime: 2022-11-10 00:04:36
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
int main(int argc, char *argv[]) {
    // int port = 443;
    // if (argc == 2) {
    //     port = atoi(argv[1]);
    // }
    WebServer httpServer(80, false, 3, 60000, false, 3);  // port, openSSL, ET, timeout, linger, threadNum
    WebServer httpsServer(443, true, 3, 60000, false, 3); // port, openSSL, ET, timeout, linger, threadNum
    auto thread1 = std::thread([&] {
        std::cout << "thread 1" << std::endl;
        httpServer.start();
    });
    auto thread2 = std::thread([&] {
        std::cout << "thread 2" << std::endl;
        httpsServer.start();
    });
    thread1.join();
    thread2.join();
}