#include <chrono>
#include <condition_variable>
#include <mutex>

class Waiter {
protected:
    std::mutex mutex;
    std::condition_variable cv;

public:
    void wait() {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock);
    }

    template <typename Rep, typename Period>
    auto wait_for(const std::chrono::duration<Rep, Period>& time) {
        std::unique_lock<std::mutex> lock(mutex);
        return cv.wait_for(lock, time);
    }

    template <typename Clock, typename Duration>
    auto wait_until(const std::chrono::time_point<Clock, Duration>& time) {
        std::unique_lock<std::mutex> lock(mutex);
        return cv.wait_until(lock, time);
    }

    void notify_one() {
        cv.notify_one();
    }

    void notify_all() {
        cv.notify_all();
    }
};
