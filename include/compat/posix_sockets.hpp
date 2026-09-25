#pragma once

// 这个项目最早是在 macOS 上写的，socket 部分用的都是 POSIX 接口：
// poll、sys/socket.h、fcntl 这些。Windows 上没有它们，所以这里用
// Winsock2 补一套同名、行为接近的函数，让同一份代码两个平台都能编译。
//
// Linux / macOS：下面的 POSIX 分支只是引入系统头文件，什么都不改。

#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstddef>

// Winsock 的 recv/send 返回 int，Linux 上用的是 ssize_t，给个等价别名。
using ssize_t = std::ptrdiff_t;

// Windows 上错误码在 WSAGetLastError() 里，Linux 上在 errno 里。
// 统一用这个函数取，比较的时候再按平台用对应的宏（见 socket.cpp）。
inline int last_error() {
    return ::WSAGetLastError();
}

// poll 在 Windows 上叫 WSAPoll，winsock2.h 里已经定义好了 pollfd。
inline int poll(pollfd* fds, std::size_t nfds, int timeout) {
    return ::WSAPoll(fds, static_cast<ULONG>(nfds), timeout);
}

// close 在 Windows 上要换成 closesocket。
inline int close(int fd) {
    return ::closesocket(fd);
}

// 项目里只需要 fcntl 的"设置非阻塞"这一种用法，
// 所以这里只实现它：Windows 用 ioctlsocket(FIONBIO)。
#define O_NONBLOCK 1
#define F_GETFL 3
#define F_SETFL 4

inline int fcntl(int fd, int cmd, int arg) {
    if (cmd == F_GETFL) {
        return 0; // 代码里只会在 F_SETFL 之前调用它，返回值没有实际用途
    }
    u_long mode = (arg & O_NONBLOCK) ? 1UL : 0UL;
    return ::ioctlsocket(fd, FIONBIO, &mode) == 0 ? 0 : -1;
}

// Winsock 的 setsockopt/getsockopt 里 optval 是 char*，
// POSIX 是 void*。套一层 POSIX 签名，调用处就不用写强制转换了。
inline int setsockopt(int s, int level, int optname, const void* optval, socklen_t optlen) {
    return ::setsockopt(static_cast<SOCKET>(s), level, optname,
                        static_cast<const char*>(optval), static_cast<int>(optlen));
}

inline int getsockopt(int s, int level, int optname, void* optval, socklen_t* optlen) {
    return ::getsockopt(static_cast<SOCKET>(s), level, optname,
                        static_cast<char*>(optval), reinterpret_cast<int*>(optlen));
}

// MSG_NOSIGNAL 是 Linux 的东西（防止对端断开时进程收到 SIGPIPE），
// Windows 的 send 不支持，保持未定义让 socket.cpp 走 flags == 0。
#ifdef MSG_NOSIGNAL
#undef MSG_NOSIGNAL
#endif

#else // Linux / macOS

#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

inline int last_error() {
    return errno;
}

#endif
