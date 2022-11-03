#pragma once

#include <cstddef>
#include <cstdint>
#include <sys/epoll.h>
#include <vector>

class Epoller {
public:
    explicit Epoller(size_t maxEvent = 1024);
    ~Epoller();

    bool addFd(int fd, uint32_t events);
    bool modFd(int fd, uint32_t events);
    bool delFd(int fd);
    int wait(int timeout = -1); // ms. timeiut=-1 means block

    int getEventFd(size_t i) const;
    uint32_t getEvents(size_t i) const;

private:
    int epollFd_;
    std::vector<struct epoll_event> epollEvents_;
};
