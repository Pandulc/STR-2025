#include <atomic>
#include <chrono>
#include <thread>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <condition_variable>
#include "supervisor.h"

using namespace std;
using namespace chrono;

// Estructura para almacenar información de cada hilo trabajador
struct ThreadInfo {
    microseconds expected_duration;
    time_point<steady_clock> last_start_time;
    atomic<bool> is_running;
    function<void()> recovery_function;
    uint32_t timeout_count{0};
};


ThreadSupervisor::ThreadSupervisor() : supervisor_thread(&ThreadSupervisor::monitor_threads, this) {}
ThreadSupervisor::~ThreadSupervisor() {
    shutdown = true;
    supervisor_thread.join();
}

void ThreadSupervisor::register_thread(int thread_id, microseconds expected_time, function<void()> recovery_func) {
    lock_guard<mutex> lock(threads_mutex);
    threads_info.emplace(
        piecewise_construct,
        forward_as_tuple(thread_id),
        forward_as_tuple(
            expected_time,
            steady_clock::now(),
            false,
            recovery_func,
            0
        )
    );
}

void ThreadSupervisor::notify_start(int thread_id) {
    lock_guard<mutex> lock(threads_mutex);
    if(threads_info.count(thread_id)) {
        threads_info[thread_id].last_start_time = steady_clock::now();
        threads_info[thread_id].is_running = true;
    }
}

void ThreadSupervisor::notify_end(int thread_id) {
    lock_guard<mutex> lock(threads_mutex);
    if(threads_info.count(thread_id)) {
        threads_info[thread_id].is_running = false;
        threads_info[thread_id].timeout_count = 0; // Reset counter on successful completion
    }
}

void ThreadSupervisor::recovery_thread(int thread_id) {
    lock_guard<mutex> lock(threads_mutex);
    if(threads_info.count(thread_id)) {
        auto& info = threads_info[thread_id];
            info.recovery_function();
            info.timeout_count = 0; // Reset counter after recovery
        // if(info.timeout_count > max_retries) {
        // }
    }
}

void ThreadSupervisor::monitor_threads() {
    while(!shutdown) {
        this_thread::sleep_for(check_interval);
            
        lock_guard<mutex> lock(threads_mutex);
        auto now = steady_clock::now();
        
        for(auto& [id, info] : threads_info) {
            if(info.is_running) {
                auto elapsed = duration_cast<microseconds>(now - info.last_start_time);
                    
                if(elapsed > info.expected_duration) {
                    info.timeout_count++;
                       
                    info.recovery_function();
                    // // Estrategia de recuperación escalonada
                    // if(info.timeout_count > max_retries) {
                    //     // Intento de recuperación suave
                    //     info.recovery_function();
                    // }
                }
            }
        }
    }
}