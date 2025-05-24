#include <iostream>
#include <thread>
#include <csignal>
#include <mutex>
#include <condition_variable>
#include <pigpio.h>
#include <opencv2/opencv.hpp>
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

atomic<int> sensor_retries(0); 

void setThreadPriority(thread& t, int priority) {
    sched_param sch_params;
    sch_params.sched_priority = priority;
    if (pthread_setschedparam(t.native_handle(), SCHED_FIFO, &sch_params)) {
        cerr << "Error al asignar prioridad" << endl;
    }
}

int main() {
    // Inicialización del supervisor
    ThreadSupervisor supervisor;

    // Inicializar GPIO
    if (gpioInitialise() < 0) {
        cerr << "Error al inicializar pigpio" << endl;
        return EXIT_FAILURE;
    }

    // Iniciar y configurar camara
    VideoCapture cam(0);
    if (!cam.isOpened()) {
        cerr << "\u274c Error: No se pudo abrir la cámara." << endl;
        //finalize = true;
        return EXIT_FAILURE;
    }
    cam.set(CAP_PROP_FRAME_WIDTH, 640);
    cam.set(CAP_PROP_FRAME_HEIGHT, 480);

    // Configurar señales
    signal(SIGUSR1, handle_signal_camera);
    
    // Configurar barrera
    pthread_barrier_init(&barrier, NULL, NUM_THREADS);

    // Definir recoverys
    auto recovery_sensor = [](atomic<int>& sensor_retries) {
        cout << "Restableciendo hilo sensor..." << endl;
        if (++sensor_retries > 5) {
            cerr << "❌ Error crítico: No se pudo recuperar el sensor. Finalizando el programa." << endl;
            exit(EXIT_FAILURE);
        }
    };

    auto recovery_camera = [&cam]() {
        cerr << "Ejecutando recuperación para cámara..." << endl;
        if(!cam.isOpened()) {
            cerr << "❌ Error crítico: No se pudo recuperar la cámara. Finalizando el programa." << endl;
            exit(EXIT_FAILURE);
        }
        cam.set(CAP_PROP_FRAME_WIDTH, 640);
        cam.set(CAP_PROP_FRAME_HEIGHT, 480);
    };

    auto recovery_communicator = []() {
        cerr << "Ejecutando recuperación para comunicador..." << endl;
        
    };

    auto recovery_barrier = []() {
        cerr << "Ejecutando recuperación para barrera..." << endl;
    };

    // Crear hilos
    supervisor.register_thread(SENSOR_THREAD, milliseconds(200), [&]() { recovery_sensor(sensor_retries); });
    supervisor.register_thread(CAMERA_THREAD, milliseconds(1000), recovery_camera);
    supervisor.register_thread(COMMUNICATOR_THREAD, milliseconds(10000), recovery_communicator);
    supervisor.register_thread(BARRIER_THREAD,milliseconds(10000), recovery_barrier);

    thread t_sensor(threadSensor, ref(supervisor), SENSOR_THREAD);
    thread t_camera(threadCamera, ref(supervisor), CAMERA_THREAD, ref(cam));
    thread t_communicator(threadCommunicator, ref(supervisor), COMMUNICATOR_THREAD);
    thread t_barrier(threadBarrier, ref(supervisor), BARRIER_THREAD);

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

