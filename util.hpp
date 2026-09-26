#pragma once

#include <FL/Fl.H>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <iostream>
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
        std::unique_lock<std::mutex> lock(mutex);
        if (!notified) {
            notified = true;
            lock.unlock();
            cv.notify_one();
        }
    }

    void notify_all() {
        std::unique_lock<std::mutex> lock(mutex);
        if (!notified) {
            notified = true;
            lock.unlock();
            cv.notify_all();
        }
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

// Never wrap the Fl::awake() calls below in assert(): NDEBUG would compile the
// dispatch itself away, silently stopping every cross-thread UI update and
// hanging the blocking overload forever
template <typename F>
void awake(F&& function, bool block = false) {
    if (block) {
        if (std::this_thread::get_id() != detail::main_thread_id) {
            detail::AwakeHelper<std::decay_t<F>> helper {std::forward<F>(function)};
            // Only wait if the message was actually queued, since a full ring
            // buffer means nothing will ever notify us
            if (Fl::awake([](void* data) {
                    auto helper = (detail::AwakeHelper<std::decay_t<F>>*) data;
                    helper->function();
                    helper->waiter.notify_one();
                },
                    &helper) == 0) {
                helper.waiter.wait();
            }
        } else {
            std::forward<F>(function)();
        }
    } else {
        auto function_copy = new std::decay_t<F>(std::forward<F>(function));
        if (Fl::awake([](void* data) {
                auto function = (std::decay_t<F>*) data;
                (*function)();
                delete function;
            },
                function_copy) != 0) {
            delete function_copy; // It was never queued, so nothing else will free it
        }
    }
}

// libdatachannel throws when a channel isn't open, and a channel can be closed
// on its own thread between isOpen() and send(), so no caller can make a send
// safe with a check alone. Returns whether the channel accepted the message;
// send()'s own bool distinguishes sent from buffered, which is not a failure
template <typename Channel, typename Message>
bool try_send(const Channel& channel, Message&& message) {
    if (!channel || !channel->isOpen()) {
        return false;
    }
    try {
        channel->send(std::forward<Message>(message));
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to send message: " << e.what() << std::endl;
        return false;
    }
}
