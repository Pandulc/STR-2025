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
const int BARRIER_PIN = 18;       // Pin para el servo o motor de la barrera
const int LED_GREEN = 17;         // Pin para el LED verde (acceso autorizado)
const int LED_RED = 27;           // Pin para el LED rojo (acceso denegado)
const int SERVO_OPEN_US = 1500;   // PWM en microsegundos para abrir (90°)
const int SERVO_CLOSED_US = 500;  // PWM en microsegundos para cerrar (0°)

/**
 * Configura los pines GPIO de la barrera y los LEDs.
 * Inicializa la barrera cerrada y enciende el LED rojo por defecto.
 */
void setupPins() {
    gpioSetMode(LED_GREEN, PI_OUTPUT);
    gpioSetMode(LED_RED, PI_OUTPUT);

    gpioServo(BARRIER_PIN, SERVO_CLOSED_US); // Barrera cerrada por defecto
    gpioWrite(LED_GREEN, 0); // LED verde apagado
    gpioWrite(LED_RED, 1);   // LED rojo encendido
}

/**
 * Función de hilo que controla la barrera de acceso mediante un servo.
 * @param supervisor Referencia al supervisor de hilos
 * @param running Variable atómica que indica si el hilo debe seguir ejecutándose
 * @param thread_id Identificador del hilo
 * 
 * El hilo espera una señal de sincronización (barrier), luego actúa según
 * el valor de `lift_barrier` (variable compartida):
 * - Si es true, abre la barrera y enciende el LED verde.
 * - Si es false, parpadea el LED rojo indicando acceso denegado.
 */
void threadBarrier(ThreadSupervisor& supervisor, atomic<bool>& running, int thread_id) {
    setupPins();

    // Asegura que el LED rojo esté encendido al iniciar
    gpioWrite(LED_RED, 1);
    gpioWrite(LED_GREEN, 0);

    while (running) {
        // Espera sincronizada con otros hilos (por ejemplo, el sensor)
        pthread_barrier_wait(&barrier);
        supervisor.notify_start(thread_id);

        if (lift_barrier) {
            cout << "\u2705 Acceso autorizado. Abriendo barrera…" << endl;

            // Cambia la luz a verde
            gpioWrite(LED_RED, 0);
            gpioWrite(LED_GREEN, 1);

            // Abre la barrera (servo a 90°)
            int status = gpioServo(BARRIER_PIN, SERVO_OPEN_US);
            cout << "Status del servo: " << status << endl;

            if (status != 0) {
                cerr << "Error al enviar PWM al servo" << endl;
                supervisor.recovery_thread(thread_id);
                gpioWrite(LED_GREEN, 0);
                gpioWrite(LED_RED, 1);
                continue;
            }

            // Espera mientras la barrera está abierta
            sleep(3);

            // Cierra la barrera (servo a 0°)
            gpioServo(BARRIER_PIN, SERVO_CLOSED_US);

            // Vuelve a encender el LED rojo
            gpioWrite(LED_GREEN, 0);
            gpioWrite(LED_RED, 1);
        } else {
            cout << "\u274c Acceso denegado. Parpadeo…" << endl;

            // Parpadea el LED rojo (apagado 1 segundo)
            gpioWrite(LED_RED, 0);
            sleep(1);
            gpioWrite(LED_RED, 1);
        }

        // Notifica fin de ciclo al supervisor y duerme brevemente
        supervisor.notify_end(thread_id);
        usleep(200000); // 200 ms
    }
}
