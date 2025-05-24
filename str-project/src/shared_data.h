#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <queue>
#include <string>
#include <mutex>
#include <condition_variable>

inline std::atomic<bool> lift_barrier(false);

class SharedQueue {
private:
    std::queue<std::string> photo_paths;
    std::mutex mtx;
    std::condition_variable cv;

public:
    void push(const std::string& path) {
        std::lock_guard<std::mutex> lock(mtx);
        photo_paths.push(path);
        cv.notify_one();
    }

    std::string wait_and_pop() {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this] { return !photo_paths.empty(); });
        std::string path = photo_paths.front();
        photo_paths.pop();
        return path;
    }
};


#endif