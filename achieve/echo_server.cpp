#include "socket.hpp"
#include "task.hpp"

#include <array>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string_view>

using async_comm::AsyncSocket;
using async_comm::DetachedTask;
using async_comm::EventLoop;

DetachedTask handle_client(AsyncSocket client) {
    std::array<char, 4096> buffer{};

    while (true) {
        //没数据时协程在这里挂起，等事件循环把可读的 fd resume
        std::ptrdiff_t n = co_await client.async_read_some(buffer.data(), buffer.size());
        if (n <= 0) {
            std::cout << "[server] client disconnected\n";
            co_return;
        }
        std::string_view received(buffer.data(), static_cast<std::size_t>(n));
        std::cout << "[server] echo " << received.size() << " bytes: " << received;

        std::size_t sent = 0;
        while (sent < received.size()) {
            std::ptrdiff_t written = co_await client.async_write_some(received.substr(sent));
            if (written <= 0) {
                co_return;
            }
            sent += static_cast<std::size_t>(written);
        }
    }
}

DetachedTask accept_loop(EventLoop& loop, std::uint16_t port) {
    AsyncSocket listener = AsyncSocket::listen(loop, port);
    std::cout << "[server] listening on 127.0.0.1:" << port << '\n';

    while (true) {
        AsyncSocket client = co_await listener.async_accept();
        std::cout << "[server] accepted client fd=" << client.fd() << '\n';
        handle_client(std::move(client)).start_detached();
    }
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

    std::uint16_t port = 9000;
    if (argc >= 2) {
        port = static_cast<std::uint16_t>(std::atoi(argv[1]));
    }

    try {
        EventLoop loop;
        accept_loop(loop, port).start_detached();
        loop.run();
    } catch (const std::exception& e) {
        std::cerr << "[server error] " << e.what() << '\n';
        return 1;
    }
}
