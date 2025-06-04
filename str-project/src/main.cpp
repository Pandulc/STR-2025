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

const int SENSOR_THREAD = 1;
const int CAMERA_THREAD = 2;
const int COMMUNICATOR_THREAD = 3;
const int BARRIER_THREAD = 4;
const int NUM_THREADS = 4;
const int LED_RED = 27;

atomic<int> sensor_retries(0);

void setThreadPriority(thread &t, int priority)
{
    sched_param sch_params;
    sch_params.sched_priority = priority;
    if (pthread_setschedparam(t.native_handle(), SCHED_FIFO, &sch_params))
    {
        cerr << "Error al asignar prioridad" << endl;
    }
}

int main()
{
    // Inicialización del supervisor
    ThreadSupervisor supervisor;
    std::atomic<bool> system_running(true);
    supervisor.set_running_flag(&system_running);

    // Inicializar GPIO
    if (gpioInitialise() < 0)
    {
        cerr << "Error al inicializar pigpio" << endl;
        return EXIT_FAILURE;
    }

    // Iniciar y configurar camara
    VideoCapture cam(0);
    if (!cam.isOpened())
    {
        cerr << "\u274c Error: No se pudo abrir la cámara." << endl;
        // finalize = true;
        return EXIT_FAILURE;
    }
    cam.set(CAP_PROP_FRAME_WIDTH, 640);
    cam.set(CAP_PROP_FRAME_HEIGHT, 480);

    // Configurar señales
    signal(SIGUSR1, handle_signal_camera);

    // Configurar barrera
    pthread_barrier_init(&barrier, NULL, NUM_THREADS);

    // Definir recoverys
    auto recovery_sensor = [&](atomic<int>& sensor_retries) {
    cout << "Intentando recuperación del sensor..." << endl;
    if (++sensor_retries > 5) {
        cerr << "❌ Error crítico: No se pudo recuperar el sensor tras varios intentos." << endl;
        enter_failsafe_state("Sensor falló de forma permanente.", supervisor);
    }

    // Reintento simple: reconfigurar GPIO
    gpioSetMode(TRIGGER_PIN, PI_OUTPUT);
    gpioSetMode(ECHO_PIN, PI_INPUT);
    gpioWrite(TRIGGER_PIN, 0);
    usleep(50000);  // tiempo para estabilizar

    cout << "✅ Reconfiguración del sensor realizada." << endl;
};

    auto recovery_camera = [&cam]()
    {
        cerr << "Ejecutando recuperación para cámara..." << endl;
        if (!cam.isOpened())
        {
            cerr << "❌ Error crítico: No se pudo recuperar la cámara. Finalizando el programa." << endl;
            enter_failsafe_state("Cámara falló de forma permanente.");
        }

        cam.set(CAP_PROP_FRAME_WIDTH, 640);
        cam.set(CAP_PROP_FRAME_HEIGHT, 480);
        
        cout << "✅ Reconfiguración de la camara realizada." << endl;
    };

    auto recovery_communicator = []()
    {
        cerr << "Ejecutando recuperación para comunicador..." << endl;

        CURL *curl = curl_easy_init();
        if (!curl)
        {
            cerr << "Error inicializando CURL" << endl;
            enter_failsafe_state("Error crítico: Comunicador inalcanzable.");
        }

        curl_easy_setopt(curl, CURLOPT_URL, "http://192.168.0.103:5000/status");
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L);
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); // HEAD request

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK)
        {
            cerr << "No se pudo contactar con el backend: " << curl_easy_strerror(res) << endl;
            enter_failsafe_state("Error crítico: Comunicador inalcanzable.");
        }

        return;
    };

    std::atomic<int> barrier_retries = 0;

auto recovery_barrier = [&]() {
    cerr << "Intentando recuperación de la barrera..." << endl;

    if (++barrier_retries > 1) {
        cerr << "❌ Error crítico: Fallo persistente en la barrera." << endl;
        enter_failsafe_state("Fallo en la barrera.", supervisor);
    }

    // Reconfiguración de los pines
    gpioSetMode(BARRIER_PIN, PI_OUTPUT);
    gpioSetMode(LED_GREEN, PI_OUTPUT);
    gpioSetMode(LED_RED, PI_OUTPUT);

    gpioWrite(BARRIER_PIN, 0); // Barrera cerrada por defecto
    gpioWrite(LED_GREEN, 0);
    gpioWrite(LED_RED, 0);

    cout << "✅ Barrera reiniciada." << endl;
};


    void enter_failsafe_state(const string &reason, ThreadSupervisor &supervisor)
    {
        cerr << "[FAILSAFE] " << reason << endl;

        supervisor.shutdown_all(); // Señal de parada cooperativa

        gpioSetMode(LED_RED, PI_OUTPUT);
        while (true)
        {
            gpioWrite(LED_RED, 1);
            this_thread::sleep_for(chrono::milliseconds(500));
            gpioWrite(LED_RED, 0);
            this_thread::sleep_for(chrono::milliseconds(500));
        }
    }

    // Crear hilos
    supervisor.register_thread(SENSOR_THREAD, milliseconds(200), [&]()
                               { recovery_sensor(sensor_retries); });
    supervisor.register_thread(CAMERA_THREAD, milliseconds(1000), recovery_camera);
    supervisor.register_thread(COMMUNICATOR_THREAD, milliseconds(10000), recovery_communicator);
    supervisor.register_thread(BARRIER_THREAD, milliseconds(10000), recovery_barrier);

    thread t_sensor(threadSensor, ref(supervisor), ref(system_running), SENSOR_THREAD);
    thread t_camera(threadCamera, ref(supervisor), ref(system_running), CAMERA_THREAD, ref(cam));
    thread t_communicator(threadCommunicator, ref(supervisor), ref(system_running), COMMUNICATOR_THREAD);
    thread t_barrier(threadBarrier, ref(supervisor), ref(system_running), BARRIER_THREAD);

    // Esperar eventos o ciclo principal (opcional)
    cout << "Sistema iniciado. Esperando eventos...\n";

    // Join threads antes de salir
    t_sensor.join();
    t_camera.join();
    t_communicator.join();
    t_barrier.join();

    // Destruir la barrera
    pthread_barrier_destroy(&barrier);

    cout << "Sistema finalizado.\n";
    return EXIT_SUCCESS;
}
