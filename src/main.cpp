/*
 * @Author: starrysky9959 965105951@qq.com
 * @Date: 2022-10-16 22:30:41
 * @LastEditors: starrysky9959 965105951@qq.com
 * @LastEditTime: 2022-11-03 00:55:58
 * @Description: 
 */
#include <queue>
#include <string>
#include <sys/types.h>
#include <vector>
#include "server/WebServer.h"
int main() {
    WebServer server(80, 3, 60000, false, // port, ET, timeout, linger,
                     6);                // threadNum
    server.start();
}