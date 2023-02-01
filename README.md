<!--
 * @Author: starrysky9959 965105951@qq.com
 * @Date: 2022-10-16 15:14:19
 * @LastEditors: starrysky9959 starrysky9651@outlook.com
 * @LastEditTime: 2023-02-02 01:21:28
 * @Description: 
-->
# HTTP/HTTPS Server
C++实现的HTTP/HTTPS服务器，master分支在[lab分支](https://github.com/starrysky9959/http_server/tree/lab)满足原有实验要求的基础上进一步完善而来。

## Features
- 支持HTTP（默认80端口）和HTTPS（默认443端口），支持get和post方法，支持的状态码包括：200，206，301，400，403，404。
- 利用IO复用技术Epoll和线程池实现多线程的Reactor高并发模型
- 利用状态机和正则表达式解析请求报文
- 利用标准库容器封装char实现自动增长的缓冲区

## Requirements
- Linux
- C++17
- CMake

## Build and Run
``` bash
mkdir build
cd build
cmake ..
make
cd ..
sudo build/lab1 # use 80 and 443 port by default
```

## Directory tree
项目整体目录结构如下所示。
``` 
.
├── CMakeLists.txt                  # 使用CMake构建本项目
├── LICENSE
├── README.md
├── WebBench                        # WebBench项目用于性能测试
├── assets
│   ├── test-init.png
│   └── test-start.png
├── keys                            # openssl生成的自签名证书
│   ├── cert.csr
│   ├── cert.pem
│   ├── cnlab.cert
│   ├── cnlab.prikey
│   └── privkey.pem
├── log                             # 日志目录
├── resources                       # 资源文件和用于测试的Python文件
│   ├── image_test.html
│   ├── images
│   ├── index.html
│   ├── test
│   ├── topo.py
│   ├── video
│   └── video_test.html
└── src                             # 源码
    ├── buffer                      # 缓冲区
    │   ├── Buffer.cpp
    │   └── Buffer.h
    ├── http                        # 处理HTTP Request和Response报文
    │   ├── HttpConnection.cpp
    │   ├── HttpConnection.h
    │   ├── HttpRequest.cpp
    │   ├── HttpRequest.h
    │   ├── HttpResponse.cpp
    │   └── HttpResponse.h
    ├── main.cpp                    # 主程序
    ├── pool                        # 线程池
    │   └── ThreadPool.h
    ├── server                      # Server整体流程和对epoll API的封装
    │   ├── Epoller.cpp
    │   ├── Epoller.h
    │   ├── WebServer.cpp
    │   └── WebServer.h
    └── util                        # 公用代码
        └── Util.h     
```

## Benchmark
使用[WebBench](https://github.com/EZLippi/WebBench)进行压力测试。在10000个连接的情况下，QPS接近10000。
- 测试环境: WSL2 Ubuntu 20.04 LTS. AMD R7-6800H. 8C16G
- 并发连接总数: 10000
- 访问服务器时间: 5s
- 每秒钟响应请求数: 591660 pages/min
- 每秒钟传输数据量: 14574558 bytes/sec
- 所有访问均成功

```bash
# 10000个连接, 测试5s
$ ./webbench -c 10000 -t 5 http://127.0.0.1/index.html
Webbench - Simple Web Benchmark 1.5
Copyright (c) Radim Kolar 1997-2004, GPL Open Source Software.

Request:
GET /index.html HTTP/1.0
User-Agent: WebBench 1.5
Host: 127.0.0.1


Runing info: 10000 clients, running 5 sec.

Speed=591660 pages/min, 14574558 bytes/sec.
Requests: 49305 susceed, 0 failed.
```


## 参考
[https://github.com/markparticle/WebServer](https://github.com/markparticle/WebServer)

[https://github.com/qinguoyi/TinyWebServer](https://github.com/qinguoyi/TinyWebServer)