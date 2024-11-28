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

    void notify_one() {
        cv.notify_one();
    }

    void notify_all() {
        cv.notify_all();
    }
};
