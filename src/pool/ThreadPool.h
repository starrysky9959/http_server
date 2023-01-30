/*
 * @Author:starrysky
 * @Date:2022-10-19
*/
#pragma once

#include <condition_variable>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <ostream>
#include <queue>
#include <system_error>
#include <thread>
#include <assert.h>
#include <utility>

class ThreadPool {
public:
    explicit ThreadPool(size_t threadCount = 8) :
        pool_(std::make_shared<Pool>()) {
        assert(threadCount > 0);
        for (size_t i = 0; i < threadCount; ++i) {
            std::thread([&] {
                std::unique_lock<std::mutex> locker(pool_->lock_);
                while (true) {
                    if (!pool_->taskQueue_.empty()) {
                        auto task = std::move(pool_->taskQueue_.front());
                        pool_->taskQueue_.pop();
                        locker.unlock();
                        task();
                        locker.lock();
                    } else if (pool_->isClosed_) {
                        break;
                    } else {
                        pool_->cond_.wait(locker);
                    }
                }
            }).detach(); // not block main thread
        }
    }

    ThreadPool() = default;

    ThreadPool(ThreadPool &&) = default;

    ~ThreadPool() {
        if (static_cast<bool>(pool_)) {
            {
                std::lock_guard<std::mutex> locker(pool_->lock_);
                pool_->isClosed_ = true;
            }
            pool_->cond_.notify_all();
        }
    }

    template <class T>
    void addTask(T &&task) {
        {
            std::lock_guard<std::mutex> locker(pool_->lock_);
            pool_->taskQueue_.emplace(std::forward<T>(task));
        }
        pool_->cond_.notify_one();
    }

private:
    class Pool {
    public:
        std::mutex lock_;
        std::condition_variable cond_;
        bool isClosed_;
        std::queue<std::function<void()>> taskQueue_;
    };
    std::shared_ptr<Pool> pool_;
};