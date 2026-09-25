#pragma once
#include "event_loop.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace async_comm {

// 把 fd 设成非阻塞：这样 recv/send/accept 没数据时不会卡住， 立刻返回 EAGAIN，才有机会把协程交给事件循环
void set_non_blocking(int fd);

class AsyncSocket {
public:
    AsyncSocket() = default;
    AsyncSocket(EventLoop& loop, int fd);
    ~AsyncSocket();
    AsyncSocket(const AsyncSocket&) = delete;
    AsyncSocket& operator=(const AsyncSocket&) = delete;
    AsyncSocket(AsyncSocket&& other) noexcept;
    AsyncSocket& operator=(AsyncSocket&& other) noexcept;

    static AsyncSocket listen(EventLoop& loop, std::uint16_t port, int backlog = 128);
    static AsyncSocket tcp(EventLoop& loop);

    int fd() const noexcept;

    class AcceptAwaiter;
    class ConnectAwaiter;
    class ReadSomeAwaiter;
    class WriteSomeAwaiter;

    AcceptAwaiter async_accept();
    ConnectAwaiter async_connect(std::string host, std::uint16_t port);
    ReadSomeAwaiter async_read_some(char* buffer, std::size_t size);
    WriteSomeAwaiter async_write_some(std::string_view data);

private:
    EventLoop* loop_ = nullptr;
    int fd_ = -1;
};

class AsyncSocket::AcceptAwaiter {
public:
    AcceptAwaiter(EventLoop& loop, int listen_fd);

    bool await_ready();
    void await_suspend(std::coroutine_handle<> handle);
    AsyncSocket await_resume();

private:
    int try_accept();

    EventLoop& loop_;
    int listen_fd_;
    int accepted_fd_ = -1;
};

class AsyncSocket::ConnectAwaiter {
public:
    ConnectAwaiter(EventLoop& loop, int fd, std::string host, std::uint16_t port);

    bool await_ready();
    void await_suspend(std::coroutine_handle<> handle);
    void await_resume();

private:
    EventLoop& loop_;
    int fd_;
    std::string host_;
    std::uint16_t port_;
};

class AsyncSocket::ReadSomeAwaiter {
public:
    ReadSomeAwaiter(EventLoop& loop, int fd, char* buffer, std::size_t size);

    bool await_ready();
    void await_suspend(std::coroutine_handle<> handle);
    std::ptrdiff_t await_resume();

private:
    std::ptrdiff_t try_read();

    EventLoop& loop_;
    int fd_;
    char* buffer_;
    std::size_t size_;
    std::ptrdiff_t result_ = -1;
};

class AsyncSocket::WriteSomeAwaiter {
public:
    WriteSomeAwaiter(EventLoop& loop, int fd, std::string_view data);

    bool await_ready();
    void await_suspend(std::coroutine_handle<> handle);
    std::ptrdiff_t await_resume();

private:
    std::ptrdiff_t try_write();

    EventLoop& loop_;
    int fd_;
    std::string_view data_;
    std::ptrdiff_t result_ = -1;
};

} // namespace async_comm
