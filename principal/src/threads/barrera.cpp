#include "barrera.h"
#include <iostream>
#include <pigpio.h>
#include <unistd.h>
#include "supervisor.h"
#include "../shared_data.h"
#include <atomic>

using namespace std;

extern pthread_barrier_t barrier;

// Pines GPIO
const int BARRIER_PIN = 18;  // Motor o servo
const int LED_GREEN = 17;
const int LED_RED = 27;
const int SERVO_OPEN_US = 1500;  // 90 grados
const int SERVO_CLOSED_US = 500; // 0 grados

void setupPins() {
    //gpioSetMode(BARRIER_PIN, PI_OUTPUT);
    gpioSetMode(LED_GREEN, PI_OUTPUT);
    gpioSetMode(LED_RED, PI_OUTPUT);

    gpioServo(BARRIER_PIN, SERVO_CLOSED_US); // Barrera cerrada por defecto
    gpioWrite(LED_GREEN, 0);
    gpioWrite(LED_RED, 1);
}

void threadBarrier(ThreadSupervisor& supervisor, atomic<bool>& running, int thread_id) {
    setupPins();

    // Encendemos el LED en rojo permanentemente
    gpioWrite(LED_RED, 1);
    gpioWrite(LED_GREEN, 0);

    while (running) {
        // Sincronizamos el inicio de todos los hilos
        pthread_barrier_wait(&barrier);
        supervisor.notify_start(thread_id);

        if (lift_barrier) {
            cout << "\u2705 Acceso autorizado. Abriendo barrera…" << endl;

            // Encendemos verde mientras esté abierta
            gpioWrite(LED_RED, 0);
            gpioWrite(LED_GREEN, 1);

            int status = gpioServo(BARRIER_PIN, SERVO_OPEN_US); // Mover a 90 grados (abrir)
            cout << "Status del servo: " << status << endl;
            if (status != 0) {
                cerr << "Error al enviar PWM al servo" << endl;
                supervisor.recovery_thread(thread_id);
                gpioWrite(LED_GREEN, 0);
                gpioWrite(LED_RED, 1);
                continue;
            }

            sleep(3);
            // Cerramos la barrera
            gpioServo(BARRIER_PIN, SERVO_CLOSED_US); // Volver a 0 grados (cerrar)

            // Volvemos el LED a rojo
            gpioWrite(LED_GREEN, 0);
            gpioWrite(LED_RED, 1);
        }
        else {
            cout << "\u274c Acceso denegado. Parpadeo…" << endl;

            // Apagamos el LED en rojo un segundo
            gpioWrite(LED_RED, 0);
            sleep(1);
            // Volvemos a encender el LED en rojo
            gpioWrite(LED_RED, 1);
        }

        supervisor.notify_end(thread_id);
        usleep(200000);
    }
}