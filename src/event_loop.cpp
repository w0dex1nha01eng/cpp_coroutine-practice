#include "event_loop.hpp"
#include <stdexcept>
#include <system_error>
#include <vector>
namespace async_comm {
void EventLoop::wait_read(int fd, std::coroutine_handle<> handle) {
    waiters_[fd].read = handle;
}

void EventLoop::wait_write(int fd, std::coroutine_handle<> handle) {
    waiters_[fd].write = handle;
}

void EventLoop::run() {
    stopped_ = false;
    while (!stopped_ && !waiters_.empty()) {
        std::vector<pollfd> poll_fds;
        poll_fds.reserve(waiters_.size());

        for (const auto& [fd, waiter] : waiters_) {
            short events = 0;
            if (waiter.read) {
                events |= POLLIN;
            }
            if (waiter.write) {
                events |= POLLOUT;
            }
            poll_fds.push_back(pollfd{static_cast<decltype(pollfd::fd)>(fd), events, 0});
        }

        int ready = ::poll(poll_fds.data(), poll_fds.size(), -1);
        if (ready < 0) {
            const int e = last_error();
#ifdef _WIN32
            const bool interrupted = (e == WSAEINTR);
#else
            const bool interrupted = (e == EINTR);
#endif
            if (interrupted) {
                continue;
            }
            throw std::system_error(e, std::system_category(), "poll failed");
        }

        std::vector<std::coroutine_handle<>> to_resume;

        for (const auto& pfd : poll_fds) {
            auto it = waiters_.find(pfd.fd);
            if (it == waiters_.end()) {
                continue;
            }

            const bool has_error = (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0;

            // 把就绪的 handle 先收集起来，等一等poll结果处理完再统一 resume（不然遍历好像会被打断）
            if ((pfd.revents & POLLIN) || has_error) {
                if (it->second.read) {
                    to_resume.push_back(it->second.read);
                    it->second.read = {};
                }
            }

            if ((pfd.revents & POLLOUT) || has_error) {
                if (it->second.write) {
                    to_resume.push_back(it->second.write);
                    it->second.write = {};
                }
            }

            if (!it->second.read && !it->second.write) {
                waiters_.erase(it);
            }
        }

        for (auto handle : to_resume) {
            if (handle && !handle.done()) {
                handle.resume();
            }
        }
    }
}

void EventLoop::stop() {
    stopped_ = true;
}

}
