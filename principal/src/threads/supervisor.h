#ifndef SUPERVISOR_H
#define SUPERVISOR_H

#include <atomic>
#include <chrono>
#include <thread>
#include <unordered_map>
#include <functional>
#include <mutex>

using namespace std;

class ThreadSupervisor {
public:
    // Constructor y destructor
    ThreadSupervisor();
    ~ThreadSupervisor();

    // Métodos públicos
    void register_thread(int thread_id, 
                        std::chrono::microseconds expected_time, 
                        std::function<void()> recovery_func);
    
    void notify_start(int thread_id);
    void notify_end(int thread_id);
    void recovery_thread(int thread_id);
    void set_running_flag(std::atomic<bool>* flag);
    void shutdown_all();

    // Eliminar copias (opcional pero recomendado)
    ThreadSupervisor(const ThreadSupervisor&) = delete;
    ThreadSupervisor& operator=(const ThreadSupervisor&) = delete;

private:
    // Estructura interna con constructor por defecto
    struct ThreadInfo {
        std::chrono::microseconds expected_duration{0};
        std::chrono::steady_clock::time_point last_start_time{};
        std::atomic<bool> is_running{false};
        std::function<void()> recovery_function{nullptr};
        uint32_t timeout_count{0};

        // Constructor por defecto
        ThreadInfo() = default;
        
        // Constructor con parámetros
        ThreadInfo(std::chrono::microseconds duration,
                 std::chrono::steady_clock::time_point start,
                 bool running,
                 std::function<void()> func,
                 uint32_t count = 0)
            : expected_duration(duration),
              last_start_time(start),
              is_running(running),
              recovery_function(std::move(func)),
              timeout_count(count) {}
    };

    // Métodos privados
    void monitor_threads();
    // void set_running_flag(std::atomic<bool>* flag);
    // void shutdown_all();

    // Miembros privados
    std::unordered_map<int, ThreadInfo> threads_info;
    std::mutex threads_mutex;
    std::thread supervisor_thread;
    std::atomic<bool>* running_flag = nullptr;
    std::atomic<bool> shutdown{false};
    const std::chrono::milliseconds check_interval{50};
    const uint32_t max_retries{3};
};

#endif // SUPERVISOR_H