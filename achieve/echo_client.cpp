#include "socket.hpp"
#include "task.hpp"

#include <array>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

using async_comm::AsyncSocket;
using async_comm::DetachedTask;
using async_comm::EventLoop;

DetachedTask client_session(EventLoop& loop, std::string host, std::uint16_t port, std::string message) {
    AsyncSocket socket = AsyncSocket::tcp(loop);

    //非阻塞 connect：连接完成之前，协程挂在这里。
    co_await socket.async_connect(std::move(host), port);

    std::cout << "[client] connected\n";
    //一次 send 不一定发得完，循环直到把整条消息发出去。
    std::size_t sent = 0;
    while (sent < message.size()) {
        std::ptrdiff_t n = co_await socket.async_write_some(std::string_view(message).substr(sent));
        if (n <= 0) {
            co_return;
        }
        sent += static_cast<std::size_t>(n);
    }

    //读一次回显。这里没做"读到完整回复"的判断，只演示一次收发。
    std::array<char, 4096> buffer{};
    std::ptrdiff_t n = co_await socket.async_read_some(buffer.data(), buffer.size());
    if (n > 0) {
        std::cout << "[client] received: " << std::string_view(buffer.data(), static_cast<std::size_t>(n));
    }
    loop.stop();
}

int main(int argc, char** argv) {
#ifdef SIGPIPE
    std::signal(SIGPIPE, SIG_IGN);
#endif

#ifdef _WIN32
    WSADATA wsa_data{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }
#endif

    std::string host = "127.0.0.1";
    std::uint16_t port = 9000;
    std::string message = "hello from C++20 coroutine client\n";

    if (argc >= 2) {
        host = argv[1];
    }
    if (argc >= 3) {
        port = static_cast<std::uint16_t>(std::atoi(argv[2]));
    }
    if (argc >= 4) {
        message = argv[3];
        message.push_back('\n');
    }
    try {
        EventLoop loop;
        client_session(loop, std::move(host), port, std::move(message)).start_detached();
        loop.run();
    } catch (const std::exception& e) {
        std::cerr << "[client error] " << e.what() << '\n';
        return 1;
    }
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
