#pragma once
#include <coroutine>
#include <exception>
#include <iostream>

namespace async_comm {

class DetachedTask {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type {
        //这里销毁 coroutine frame，理论上协程已经跑完，应该不会再有人 resume 它
        struct FinalAwaiter {
            bool await_ready() noexcept {
                return false;
            }

            void await_suspend(handle_type handle) noexcept {
                handle.destroy();
            }

            void await_resume() noexcept {}
        };

        DetachedTask get_return_object() noexcept {
            return DetachedTask(handle_type::from_promise(*this));
        }
        std::suspend_always initial_suspend() noexcept {
            return {};
        }

        FinalAwaiter final_suspend() noexcept {
            return {};
        }

        void return_void() noexcept {}

        void unhandled_exception() noexcept {
            try {
                throw;
            } catch (const std::exception& e) {
                std::cerr << "[coroutine exception] " << e.what() << '\n';
            } catch (...) {
                std::cerr << "[coroutine exception] unknown exception\n";
            }
        }
    };

    explicit DetachedTask(handle_type handle) noexcept
        : handle_(handle) {}

    DetachedTask(DetachedTask&& other) noexcept
        : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    DetachedTask& operator=(DetachedTask&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        if (handle_) {
            handle_.destroy();
        }
        handle_ = other.handle_;
        other.handle_ = nullptr;
        return *this;
    }

    DetachedTask(const DetachedTask&) = delete;
    DetachedTask& operator=(const DetachedTask&) = delete;

    ~DetachedTask() {
        if (handle_) {
            handle_.destroy();
        }
    }

    void start_detached() noexcept {
        auto handle = handle_;
        handle_ = nullptr;
        handle.resume();
    }

private:
    handle_type handle_ = nullptr;
};

} // namespace async_comm
