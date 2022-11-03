#include "Epoller.h"
#include <cassert>
#include <cstddef>
#include <sys/epoll.h>
#include <unistd.h>

Epoller::Epoller(size_t maxEvent) :
    epollFd_{epoll_create(512)}, epollEvents_{maxEvent} {
    assert(epollFd_ >= 0 && !epollEvents_.empty());
}
Epoller::~Epoller() {
    close(epollFd_);
}

bool Epoller::addFd(int fd, uint32_t events) {
    if (fd < 0) {
        return false;
    }
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    return epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev) == 0;
}

bool Epoller::modFd(int fd, uint32_t events) {
    if (fd < 0) {
        return false;
    }
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;
    return epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev) == 0;
}

bool Epoller::delFd(int fd) {
    if (fd < 0) {
        return false;
    }
    epoll_event ev = {0};
    return epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, &ev) == 0;
}

int Epoller::wait(int timeout) {
    return epoll_wait(epollFd_, &epollEvents_[0], static_cast<int>(epollEvents_.size()), timeout);
}

int Epoller::getEventFd(size_t i) const {
    assert(i >= 0 && i < epollEvents_.size());
    return epollEvents_[i].data.fd;
}

uint32_t Epoller::getEvents(size_t i) const {
    assert(i >= 0 && i < epollEvents_.size());
    return epollEvents_[i].events;
}