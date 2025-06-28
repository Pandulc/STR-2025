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

// Estructura que almacena la información relevante de cada hilo supervisado
struct ThreadInfo {
    microseconds expected_duration;                      // Duración máxima esperada del ciclo del hilo
    time_point<steady_clock> last_start_time;            // Última vez que el hilo comenzó a ejecutarse
    atomic<bool> is_running;                             // Indica si el hilo está actualmente ejecutándose
    function<void()> recovery_function;                  // Función de recuperación en caso de timeout
    uint32_t timeout_count{0};                           // Contador de timeouts consecutivos
};

/**
 * Constructor de la clase ThreadSupervisor.
 * Inicializa el hilo de supervisión para monitorear los hilos registrados.
 */
ThreadSupervisor::ThreadSupervisor() 
    : supervisor_thread(&ThreadSupervisor::monitor_threads, this) {}

/**
 * Destructor de la clase ThreadSupervisor.
 * Señala el cierre y espera a que el hilo de monitoreo finalice.
 */
ThreadSupervisor::~ThreadSupervisor() {
    shutdown = true;
    supervisor_thread.join();
}

/**
 * Asocia una bandera atómica externa para controlar el ciclo de ejecución general.
 * @param flag Puntero a una variable atómica compartida.
 */
void ThreadSupervisor::set_running_flag(atomic<bool>* flag) {
    running_flag = flag;
}

/**
 * Detiene la ejecución de todos los hilos registrados.
 * Establece las banderas `is_running` en falso para todos los hilos.
 */
void ThreadSupervisor::shutdown_all() {
    if (running_flag) *running_flag = false;

    lock_guard<mutex> lock(threads_mutex);
    for (auto& [id, info] : threads_info) {
        info.is_running = false;
    }
}

/**
 * Registra un nuevo hilo en el sistema de supervisión.
 * @param thread_id ID único del hilo.
 * @param expected_time Tiempo máximo esperado por iteración.
 * @param recovery_func Función de recuperación ante un posible cuelgue.
 */
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

/**
 * Notifica que un hilo ha comenzado su ejecución.
 * @param thread_id ID del hilo que inicia.
 */
void ThreadSupervisor::notify_start(int thread_id) {
    lock_guard<mutex> lock(threads_mutex);
    if (threads_info.count(thread_id)) {
        threads_info[thread_id].last_start_time = steady_clock::now();
        threads_info[thread_id].is_running = true;
    }
}

/**
 * Notifica que un hilo ha finalizado su ejecución correctamente.
 * Reinicia el contador de timeouts.
 * @param thread_id ID del hilo que termina.
 */
void ThreadSupervisor::notify_end(int thread_id) {
    lock_guard<mutex> lock(threads_mutex);
    if (threads_info.count(thread_id)) {
        threads_info[thread_id].is_running = false;
        threads_info[thread_id].timeout_count = 0;
    }
}

/**
 * Ejecuta la función de recuperación asociada a un hilo específico.
 * Reinicia el contador de timeouts de ese hilo.
 * @param thread_id ID del hilo a recuperar.
 */
void ThreadSupervisor::recovery_thread(int thread_id) {
    function<void()> func;

    {
        lock_guard<mutex> lock(threads_mutex);
        if (threads_info.count(thread_id)) {
            auto& info = threads_info[thread_id];
            func = info.recovery_function;
            info.timeout_count = 0;
        }
    }

    if (func) func();
}

/**
 * Función que ejecuta el hilo de monitoreo.
 * Verifica periódicamente si algún hilo ha superado su tiempo de ejecución esperado
 * y, de ser así, ejecuta su función de recuperación.
 */
void ThreadSupervisor::monitor_threads() {
    while (!shutdown) {
        this_thread::sleep_for(check_interval); // Intervalo de chequeo

        vector<function<void()>> recoveries;

        {
            lock_guard<mutex> lock(threads_mutex);
            auto now = steady_clock::now();

            for (auto& [id, info] : threads_info) {
                if (info.is_running) {
                    auto elapsed = duration_cast<microseconds>(now - info.last_start_time);

                    if (elapsed > info.expected_duration) {
                        info.timeout_count++;
                        recoveries.push_back(info.recovery_function);
                    }
                }
            }
        }

        // Ejecuta funciones de recuperación fuera del lock
        for (auto& func : recoveries) {
            if (func) func();
        }
    }
}
