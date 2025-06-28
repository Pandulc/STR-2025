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
                        chrono::microseconds expected_time, 
                        function<void()> recovery_func);
    
    void notify_start(int thread_id);
    void notify_end(int thread_id);
    void recovery_thread(int thread_id);
    void set_running_flag(atomic<bool>* flag);
    void shutdown_all();

    // Eliminar copias (opcional pero recomendado)
    ThreadSupervisor(const ThreadSupervisor&) = delete;
    ThreadSupervisor& operator=(const ThreadSupervisor&) = delete;

private:
    // Estructura interna con constructor por defecto
    struct ThreadInfo {
        chrono::microseconds expected_duration{0};
        chrono::steady_clock::time_point last_start_time{};
        atomic<bool> is_running{false};
        function<void()> recovery_function{nullptr};
        uint32_t timeout_count{0};

        // Constructor por defecto
        ThreadInfo() = default;
        
        // Constructor con parámetros
        ThreadInfo(chrono::microseconds duration,
                 chrono::steady_clock::time_point start,
                 bool running,
                 function<void()> func,
                 uint32_t count = 0)
            : expected_duration(duration),
              last_start_time(start),
              is_running(running),
              recovery_function(move(func)),
              timeout_count(count) {}
    };

    // Métodos privados
    void monitor_threads();

    // Miembros privados
    unordered_map<int, ThreadInfo> threads_info;
    mutex threads_mutex;
    thread supervisor_thread;
    atomic<bool>* running_flag = nullptr;
    atomic<bool> shutdown{false};
    const chrono::milliseconds check_interval{50};
    const uint32_t max_retries{3};
};

#endif // SUPERVISOR_H