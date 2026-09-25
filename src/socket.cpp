#include "socket.hpp"

#include "compat/posix_sockets.hpp"

#include <stdexcept>
#include <system_error>

namespace async_comm {
namespace {

bool would_block(int error) {
#ifdef _WIN32
    return error == WSAEWOULDBLOCK;
#else
    return error == EAGAIN || error == EWOULDBLOCK;
#endif
}
[[noreturn]] void throw_errno(const char* what) {
    throw std::system_error(last_error(), std::system_category(), what);
}

//先不做域名解析（完全不会做）
sockaddr_in make_ipv4_address(const std::string& host, std::uint16_t port) {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    if (::inet_pton(AF_INET, host.c_str(), &address.sin_addr) != 1) {
        throw std::runtime_error("only numeric IPv4 addresses are supported, for example 127.0.0.1");
    }

    return address;
}

} // namespace

void set_non_blocking(int fd) {
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        throw_errno("fcntl(F_GETFL) failed");
    }
    if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        throw_errno("fcntl(F_SETFL) failed");
    }
}

AsyncSocket::AsyncSocket(EventLoop& loop, int fd)
    : loop_(&loop), fd_(fd) {}

AsyncSocket::~AsyncSocket() {
    //以免fd还没被关
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

AsyncSocket::AsyncSocket(AsyncSocket&& other) noexcept
    : loop_(other.loop_), fd_(other.fd_) {
    other.loop_ = nullptr;
    other.fd_ = -1;
}

AsyncSocket& AsyncSocket::operator=(AsyncSocket&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    if (fd_ >= 0) {
        ::close(fd_);
    }
    loop_ = other.loop_;
    fd_ = other.fd_;
    other.loop_ = nullptr;
    other.fd_ = -1;
    return *this;
}

AsyncSocket AsyncSocket::listen(EventLoop& loop, std::uint16_t port, int backlog) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        throw_errno("socket failed");
    }
    int yes = 1;
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        ::close(fd);
        throw_errno("setsockopt(SO_REUSEADDR) failed");
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    if (::bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        ::close(fd);
        throw_errno("bind failed");
    }
    if (::listen(fd, backlog) < 0) {
        ::close(fd);
        throw_errno("listen failed");
    }

    set_non_blocking(fd);
    return AsyncSocket(loop, fd);
}

AsyncSocket AsyncSocket::tcp(EventLoop& loop) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        throw_errno("socket failed");
    }
    set_non_blocking(fd);
    return AsyncSocket(loop, fd);
}

int AsyncSocket::fd() const noexcept {
    return fd_;
}

AsyncSocket::AcceptAwaiter AsyncSocket::async_accept() {
    return AcceptAwaiter(*loop_, fd_);
}

AsyncSocket::ConnectAwaiter AsyncSocket::async_connect(std::string host, std::uint16_t port) {
    return ConnectAwaiter(*loop_, fd_, std::move(host), port);
}

AsyncSocket::ReadSomeAwaiter AsyncSocket::async_read_some(char* buffer, std::size_t size) {
    return ReadSomeAwaiter(*loop_, fd_, buffer, size);
}

AsyncSocket::WriteSomeAwaiter AsyncSocket::async_write_some(std::string_view data) {
    return WriteSomeAwaiter(*loop_, fd_, data);
}

AsyncSocket::AcceptAwaiter::AcceptAwaiter(EventLoop& loop, int listen_fd)
    : loop_(loop), listen_fd_(listen_fd) {}

int AsyncSocket::AcceptAwaiter::try_accept() {
    sockaddr_in peer{};
    socklen_t peer_len = sizeof(peer);
    int fd = ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&peer), &peer_len);
    if (fd < 0) {
        if (would_block(last_error())) {
            return -1;
        }
        throw_errno("accept failed");
    }
    set_non_blocking(fd);
    return fd;
}

bool AsyncSocket::AcceptAwaiter::await_ready() {
    // 先直接试一次：碰巧有连接就不用挂起了
    accepted_fd_ = try_accept();
    return accepted_fd_ >= 0;
}

void AsyncSocket::AcceptAwaiter::await_suspend(std::coroutine_handle<> handle) {
    //有连接进来再 resume 它
    loop_.wait_read(listen_fd_, handle);
}

AsyncSocket AsyncSocket::AcceptAwaiter::await_resume() {
    if (accepted_fd_ < 0) {
        accepted_fd_ = try_accept();
        if (accepted_fd_ < 0) {
            throw std::runtime_error("accept failed after wakeup");
        }
    }
    return AsyncSocket(loop_, accepted_fd_);
}

AsyncSocket::ConnectAwaiter::ConnectAwaiter(EventLoop& loop, int fd, std::string host, std::uint16_t port)
    : loop_(loop), fd_(fd), host_(std::move(host)), port_(port) {}

bool AsyncSocket::ConnectAwaiter::await_ready() {
    sockaddr_in address = make_ipv4_address(host_, port_);
    if (::connect(fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0) {
        return true; // 运气好，一次成功，不用挂起
    }

#ifdef _WIN32
    const bool in_progress = (last_error() == WSAEWOULDBLOCK);
#else
    const int e = last_error();
    const bool in_progress = (e == EINPROGRESS || e == EALREADY || e == EWOULDBLOCK);
#endif
    if (!in_progress) {
        throw std::system_error(last_error(), std::system_category(), "connect failed");
    }
    return false; 
}

void AsyncSocket::ConnectAwaiter::await_suspend(std::coroutine_handle<> handle) {
    loop_.wait_write(fd_, handle);
}

void AsyncSocket::ConnectAwaiter::await_resume() {
    int error = 0;
    socklen_t len = sizeof(error);
    if (::getsockopt(fd_, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
        throw_errno("getsockopt(SO_ERROR) failed");
    }
    if (error != 0) {
        throw std::system_error(error, std::system_category(), "connect failed");
    }
}

AsyncSocket::ReadSomeAwaiter::ReadSomeAwaiter(EventLoop& loop, int fd, char* buffer, std::size_t size)
    : loop_(loop), fd_(fd), buffer_(buffer), size_(size) {}

std::ptrdiff_t AsyncSocket::ReadSomeAwaiter::try_read() {
    ssize_t n = ::recv(fd_, buffer_, size_, 0);
    if (n >= 0) {
        return n;
    }
    if (would_block(last_error())) {
        return -1;
    }
    throw_errno("recv failed");
}

bool AsyncSocket::ReadSomeAwaiter::await_ready() {
    result_ = try_read();
    return result_ >= 0;
}

void AsyncSocket::ReadSomeAwaiter::await_suspend(std::coroutine_handle<> handle) {
    loop_.wait_read(fd_, handle);
}

std::ptrdiff_t AsyncSocket::ReadSomeAwaiter::await_resume() {
    if (result_ < 0) {
        result_ = try_read();
    }
    return result_;
}

AsyncSocket::WriteSomeAwaiter::WriteSomeAwaiter(EventLoop& loop, int fd, std::string_view data)
    : loop_(loop), fd_(fd), data_(data) {}

std::ptrdiff_t AsyncSocket::WriteSomeAwaiter::try_write() {
#ifdef MSG_NOSIGNAL
    constexpr int flags = MSG_NOSIGNAL;
#else
    constexpr int flags = 0;
#endif

    ssize_t n = ::send(fd_, data_.data(), data_.size(), flags);
    if (n >= 0) {
        return n;
    }
    if (would_block(last_error())) {
        return -1;
    }
    throw_errno("send failed");
}

bool AsyncSocket::WriteSomeAwaiter::await_ready() {
    result_ = try_write();
    return result_ >= 0;
}

void AsyncSocket::WriteSomeAwaiter::await_suspend(std::coroutine_handle<> handle) {
    loop_.wait_write(fd_, handle);
}

std::ptrdiff_t AsyncSocket::WriteSomeAwaiter::await_resume() {
    if (result_ < 0) {
        result_ = try_write(); 
    }
    return result_;
}

} // namespace async_comm
