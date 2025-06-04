#include "barrera.h"
#include <iostream>
#include <pigpio.h>
#include <unistd.h>
#include "supervisor.h"
#include "../shared_data.h"

using namespace std;

extern pthread_barrier_t barrier;

// Pines GPIO
const int BARRIER_PIN = 18;  // Motor o servo
const int LED_GREEN = 17;
const int LED_RED = 27;

void setupPins() {
    gpioSetMode(BARRIER_PIN, PI_OUTPUT);
    gpioSetMode(LED_GREEN, PI_OUTPUT);
    gpioSetMode(LED_RED, PI_OUTPUT);

    gpioWrite(BARRIER_PIN, 0); // Barrera cerrada por defecto
    gpioWrite(LED_GREEN, 0);
    gpioWrite(LED_RED, 0);
}

void threadBarrier(ThreadSupervisor& supervisor, std::atomic<bool>& running, int thread_id) {
    setupPins();

    while (running) {
        pthread_barrier_wait(&barrier); // Espera a que todos los hilos lleguen a la barrera
        supervisor.notify_start(thread_id);

        string resultado;
        if (lift_barrier) {
               cout << "\u2705 Acceso autorizado. Abriendo barrera..." << endl;
                gpioWrite(LED_GREEN, 1);
                gpioWrite(LED_RED, 0);
                int status = gpioWrite(BARRIER_PIN, 1); // Abrir barrera

                if (status != 0) {
                    cerr << "Error al abrir la barrera" << endl;
                    supervisor.recovery_thread(thread_id);
                    continue; // Salir del bucle y esperar a la siguiente iteración
                }

                sleep(3); // Esperar a que pase el vehículo

                gpioWrite(BARRIER_PIN, 0); // Cerrar barrera
                gpioWrite(LED_GREEN, 0);
            
        }else {
                cout << "\u274c Acceso denegado. Mostrando luz roja." << endl;
                gpioWrite(LED_GREEN, 0);
                gpioWrite(LED_RED, 1);
                sleep(2);
                gpioWrite(LED_RED, 0);
            }

        supervisor.notify_end(thread_id);
        usleep(200000); // 200ms
    }
}
