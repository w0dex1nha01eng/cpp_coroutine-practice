#pragma once
#include <coroutine>
#include <unordered_map>

#include "compat/posix_sockets.hpp"

namespace async_comm {

class EventLoop {
public:
    EventLoop() = default;

    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    // 协程暂时没法继续时，把 handle 记在对应 fd 上。
    void wait_read(int fd, std::coroutine_handle<> handle);
    void wait_write(int fd, std::coroutine_handle<> handle);

    void run();
    void stop();

private:
    struct Waiter {
        std::coroutine_handle<> read;
        std::coroutine_handle<> write;
    };

    std::unordered_map<int, Waiter> waiters_;
    bool stopped_ = false;
};

} // namespace async_comm
