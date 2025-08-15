#pragma once

#include <FL/Fl.H>
#include <assert.h>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>

class Waiter {
protected:
    std::mutex mutex;
    std::condition_variable cv;
    bool notified = false;

public:
    void wait() {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [this] {
            return notified;
        });
        notified = false;
    }

    template <typename Rep, typename Period>
    bool wait_for(const std::chrono::duration<Rep, Period>& time) {
        std::unique_lock<std::mutex> lock(mutex);
        bool result = cv.wait_for(lock, time, [this] {
            return notified;
        });
        if (result) notified = false;
        return result;
    }

    template <typename Clock, typename Duration>
    bool wait_until(const std::chrono::time_point<Clock, Duration>& time) {
        std::unique_lock<std::mutex> lock(mutex);
        bool result = cv.wait_until(lock, time, [this] {
            return notified;
        });
        if (result) notified = false;
        return result;
    }

    void notify_one() {
        mutex.lock();
        notified = true;
        mutex.unlock();
        cv.notify_one();
    }

    void notify_all() {
        mutex.lock();
        notified = true;
        mutex.unlock();
        cv.notify_all();
    }
};

namespace detail {
    extern std::thread::id main_thread_id;

    template <typename F>
    struct AwakeHelper {
        F function;
        Waiter waiter;
    };
} // namespace detail

template <typename F>
void awake(F&& function, bool block = false) {
    if (block) {
        if (std::this_thread::get_id() != detail::main_thread_id) {
            detail::AwakeHelper<std::decay_t<F>> helper {std::forward<F>(function)};
            assert(Fl::awake([](void* data) {
                auto helper = (detail::AwakeHelper<std::decay_t<F>>*) data;
                helper->function();
                helper->waiter.notify_one();
            },
                       &helper) == 0);
            helper.waiter.wait();
        } else {
            std::forward<F>(function)();
        }
    } else {
        assert(Fl::awake([](void* data) {
            auto function = (std::decay_t<F>*) data;
            (*function)();
            delete function;
        },
                   new std::decay_t<F>(std::forward<F>(function))) == 0);
    }
}
