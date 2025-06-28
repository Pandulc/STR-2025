#include <iostream>
#include <thread>
#include <csignal>
#include <mutex>
#include <condition_variable>
#include <pigpio.h>
#include <opencv2/opencv.hpp>
#include <curl/curl.h>
#include <vector>
#include <atomic>
#include <pthread.h>

// Tus headers
#include "threads/sensor.h"
#include "threads/camera.h"
#include "threads/supervisor.h"
#include "threads/communicator.h"
#include "threads/barrera.h"
#include "shared_data.h"

pthread_barrier_t barrier;
SharedQueue sharedQueue;

using namespace cv;
using namespace std;
using namespace chrono;

// Constantes y variables globales
const int SENSOR_THREAD = 1;
const int CAMERA_THREAD = 2;
const int COMMUNICATOR_THREAD = 3;
const int BARRIER_THREAD = 4;
const int NUM_THREADS = 4;
const int LED_RED = 27;
const int LED_GREEN = 17;
const int BARRIER_PIN = 18;
const int TRIGGER_PIN = 23;
const int ECHO_PIN = 24;

atomic<bool> system_running(true);
ThreadSupervisor* global_supervisor_ptr = nullptr;

void setThreadPriority(thread &t, int priority) {
    sched_param sch_params;
    sch_params.sched_priority = priority;
    if (pthread_setschedparam(t.native_handle(), SCHED_FIFO, &sch_params)) {
        cerr << "Error al asignar prioridad" << endl;
    }
}

// Signal handler para Ctrl+C
void handle_sigint(int signum) {
    cout << "\n🛑 Ctrl+C detectado. Apagando sistema..." << endl;

    system_running = false;

    if (global_supervisor_ptr) {
        global_supervisor_ptr->shutdown_all();
    }

    gpioWrite(LED_RED, 0);
    gpioWrite(LED_GREEN, 0);
    gpioServo(BARRIER_PIN, 0);
    
    cout << "✅ Señal de apagado enviada. Esperando que terminen los threads...\n";
}

void enter_failsafe_state(const string &reason, ThreadSupervisor &supervisor) {
    cerr << "[FAILSAFE] " << reason << endl;
    supervisor.shutdown_all();
    gpioServo(BARRIER_PIN, 500);
    gpioSetMode(LED_RED, PI_OUTPUT);
    gpioSetMode(LED_GREEN, PI_OUTPUT);
    while (true) {
        gpioWrite(LED_RED, 1);
        gpioWrite(LED_GREEN, 1);
        this_thread::sleep_for(chrono::milliseconds(500));
        gpioWrite(LED_RED, 0);
        gpioWrite(LED_GREEN, 0);
        this_thread::sleep_for(chrono::milliseconds(500));
    }
}

int main() {
    sigset_t signal_set;
    sigemptyset(&signal_set);
    sigaddset(&signal_set, SIGINT);
    
    // Bloquear SIGINT en el thread principal (todos los threads heredarán esta máscara)
    if (pthread_sigmask(SIG_BLOCK, &signal_set, NULL) != 0) {
        cerr << "Error bloqueando SIGINT en thread principal" << endl;
        return EXIT_FAILURE;
    }

    ThreadSupervisor supervisor;
    global_supervisor_ptr = &supervisor;

    supervisor.set_running_flag(&system_running);

    if (gpioInitialise() < 0) {
        cerr << "Error al inicializar pigpio" << endl;
        return EXIT_FAILURE;
    }

    VideoCapture cam(0);
    if (!cam.isOpened()) {
        cerr << "\u274c Error: No se pudo abrir la cámara." << endl;
        return EXIT_FAILURE;
    }
    cam.set(CAP_PROP_FRAME_WIDTH, 640);
    cam.set(CAP_PROP_FRAME_HEIGHT, 480);

    // Configurar señales para la cámara
    signal(SIGUSR1, handle_signal_camera);

    // Configurar barrera
    pthread_barrier_init(&barrier, NULL, NUM_THREADS);

    atomic<int> sensor_retries(0);
    auto recovery_sensor = [&supervisor, &sensor_retries] () {
        cout << "Intentando recuperación del sensor..." << endl;
        if (++sensor_retries > 5) {
            cerr << "❌ Error crítico: No se pudo recuperar el sensor tras varios intentos." << endl;
            enter_failsafe_state("Sensor falló de forma permanente.", supervisor);
        }
        gpioSetMode(TRIGGER_PIN, PI_OUTPUT);
        gpioSetMode(ECHO_PIN, PI_INPUT);
        gpioWrite(TRIGGER_PIN, 0);
        usleep(50000);
        cout << "✅ Reconfiguración del sensor realizada." << endl;
        sensor_retries = 0;
    };

    auto recovery_camera = [&cam, &supervisor]() {
        cerr << "Ejecutando recuperación para cámara..." << endl;
        if (!cam.isOpened()) {
            cerr << "❌ Error crítico: No se pudo recuperar la cámara." << endl;
            enter_failsafe_state("Cámara falló de forma permanente.", supervisor);
        }
        cam.set(CAP_PROP_FRAME_WIDTH, 640);
        cam.set(CAP_PROP_FRAME_HEIGHT, 480);
        cout << "✅ Reconfiguración de la cámara realizada." << endl;
    };

    auto recovery_communicator = [&supervisor]() {
        cerr << "Ejecutando recuperación para comunicador..." << endl;
        CURL *curl = curl_easy_init();
        if (!curl) {
            cerr << "Error inicializando CURL" << endl;
            enter_failsafe_state("Error crítico: Comunicador inalcanzable.", supervisor);
        }
        curl_easy_setopt(curl, CURLOPT_URL, "http://192.168.0.103:5000/status");
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L);
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        if (res != CURLE_OK) {
            cerr << "No se pudo contactar con el backend: " << curl_easy_strerror(res) << endl;
            enter_failsafe_state("Error crítico: Comunicador inalcanzable.", supervisor);
        }
    };

    atomic<int> barrier_retries = 0;
    auto recovery_barrier = [&supervisor, &barrier_retries]() {
        cerr << "Intentando recuperación de la barrera..." << endl;
        if (++barrier_retries > 1) {
            cerr << "❌ Error crítico: Fallo persistente en la barrera." << endl;
            enter_failsafe_state("Fallo en la barrera.", supervisor);
        }
        gpioSetMode(LED_GREEN, PI_OUTPUT);
        gpioSetMode(LED_RED, PI_OUTPUT);
        gpioServo(BARRIER_PIN, 500);
        gpioWrite(LED_GREEN, 0);
        gpioWrite(LED_RED, 0);
        cout << "✅ Barrera reiniciada." << endl;
        barrier_retries = 0;
    };

    // Registrar los threads con el supervisor
    supervisor.register_thread(SENSOR_THREAD, milliseconds(200), recovery_sensor);
    supervisor.register_thread(CAMERA_THREAD, milliseconds(1000), recovery_camera);
    supervisor.register_thread(COMMUNICATOR_THREAD, milliseconds(10000), recovery_communicator);
    supervisor.register_thread(BARRIER_THREAD, milliseconds(10000), recovery_barrier);

    // Crear los threads de trabajo (heredarán la máscara de señales bloqueada)
    thread t_sensor(threadSensor, ref(supervisor), ref(system_running), SENSOR_THREAD);
    thread t_camera(threadCamera, ref(supervisor), ref(system_running), CAMERA_THREAD, ref(cam));
    thread t_communicator(threadCommunicator, ref(supervisor), ref(system_running), COMMUNICATOR_THREAD);
    thread t_barrier(threadBarrier, ref(supervisor), ref(system_running), BARRIER_THREAD);

    // Thread dedicado para manejar señales
    thread signal_thread([&signal_set]() {
        int received_signal;
        cout << "🔧 Thread de señales iniciado. Esperando Ctrl+C..." << endl;
        
        // Esperar por SIGINT
        int result = sigwait(&signal_set, &received_signal);
        if (result == 0 && received_signal == SIGINT) {
            cout << "\n🛑 SIGINT recibida en thread dedicado. Iniciando apagado..." << endl;
            handle_sigint(received_signal);
        } else {
            cerr << "Error en sigwait: " << result << endl;
        }
    });

    cout << "Sistema iniciado. Esperando eventos...\n";
    cout << "Presiona Ctrl+C para terminar el programa.\n";

    // Esperar a que termine el signal_thread (cuando reciba Ctrl+C)
    signal_thread.join();
    
    cout << "🔧 Esperando que terminen los threads de trabajo..." << endl;

    // Dar tiempo a los threads para que vean system_running = false
    this_thread::sleep_for(chrono::milliseconds(100));

    // Esperar a que terminen los threads de trabajo
    if (t_sensor.joinable()) {
        cout << "🔧 Esperando thread sensor..." << endl;
        t_sensor.join();
    }
    if (t_camera.joinable()) {
        cout << "🔧 Esperando thread camera..." << endl;
        t_camera.join();
    }
    if (t_communicator.joinable()) {
        cout << "🔧 Esperando thread communicator..." << endl;
        t_communicator.join();
    }
    if (t_barrier.joinable()) {
        cout << "🔧 Esperando thread barrier..." << endl;
        t_barrier.join();
    }

    pthread_barrier_destroy(&barrier);
    gpioTerminate();

    cout << "✅ Apagado limpio completado.\n";
    return EXIT_SUCCESS;
}