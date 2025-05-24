#include "sensor.h"
#include <iostream>
#include <csignal>
#include <unistd.h>
#include <pigpio.h>
#include <unistd.h>
#include <chrono>
#include "supervisor.h"
#include <time.h>
#include <atomic>

using namespace std;

// Configuración de pines
const int TRIGGER_PIN = 23;                // Pin GPIO para el trigger (salida)
const int ECHO_PIN = 24;                // Pin GPIO para el echo (entrada)
const double DISTANCE_THRESHOLD_CM = 30.0; // Distancia mínima para detección (cm)
const int SENSOR_TIMEOUT_US = 100000;   // Tiempo máximo de espera para el sensor (microsegundos)
const int DEBOUNCE_TIME_S = 2;          // Tiempo mínimo entre detecciones (segundos)
const int TRIGGER_PULSE_US = 10;        // Duración del pulso de trigger (microsegundos)
const int INITIAL_DELAY_US = 2000;      // Tiempo de espera inicial (microsegundos)
const double SPEED_OF_SOUND_CM_PER_S = 34300.0; // Velocidad del sonido en cm/s

extern pthread_barrier_t barrier;

class UltrasonicSensor {
    private:
        int triggerPin;
        int echoPin;
    
    public:

        /**
         * Constructor: Configura los pines GPIO
         * @param trig Pin GPIO para el trigger
         * @param echo Pin GPIO para el echo
         */
        UltrasonicSensor(int trig, int echo) : triggerPin(trig), echoPin(echo) {
            gpioSetMode(triggerPin, PI_OUTPUT);
            gpioSetMode(echoPin, PI_INPUT);
            gpioWrite(triggerPin, 0);
            usleep(50000); // Espera 50ms para estabilizar
        }
    
        /**
        * Mide la distancia usando un sensor ultrasónico HC-SR04
        * @return Distancia en centímetros o -1 si hay error/timeout
        * @note Necesita permisos de GPIO y librería pigpio correctamente instalada
        */
        double measureDistance() {
            // 1. Envío del pulso de activación
            gpioWrite(triggerPin, 0);
            usleep(2000);
            gpioWrite(triggerPin, 1);
            usleep(10);
            gpioWrite(triggerPin, 0);
        
            // 2. Espera el inicio del eco con timeout
            const auto timeout = chrono::microseconds(SENSOR_TIMEOUT_US);
            auto start_time = chrono::steady_clock::now();
            
            while(gpioRead(echoPin) == 0) {
                if(chrono::steady_clock::now() - start_time > timeout) {
                    return -1.0;
                }
            }
            const auto pulse_start = chrono::steady_clock::now();
        
            // 3. Mide la duración del pulso de eco
            start_time = chrono::steady_clock::now();
            while(gpioRead(echoPin) == 1) {
                if(chrono::steady_clock::now() - start_time > timeout) {
                    return -1.0;
                }
            }
            const auto pulse_end = chrono::steady_clock::now();
        
            // 4. Cálculo de distancia
            constexpr double speed_of_sound_cm_per_s = 34300.0;
            const auto pulse_duration = chrono::duration<double>(pulse_end - pulse_start);
            const double distance = (pulse_duration.count() * speed_of_sound_cm_per_s) / 2.0;
        
            return distance > 0 ? distance : -1.0;
        }
    }; 

void threadSensor(ThreadSupervisor& supervisor, int thread_id) {
    UltrasonicSensor sensor(TRIGGER_PIN, ECHO_PIN);
    bool detected_car = false;
    sleep(1);
    
    try {
        time_t last_detection = 0;
        while (true) {
            // Notifica el inicio del hilo al supervisor
            supervisor.notify_start(thread_id);

            // Mide la distancia
            double distance = sensor.measureDistance();


            if (distance > 0 && distance < DISTANCE_THRESHOLD_CM && !detected_car) {
                time_t now = time(nullptr);
                detected_car = true;

                if (difftime(now, last_detection) >= DEBOUNCE_TIME_S) {
                    cout << "\u2705 Presencia detectada! Distancia: " << distance << " cm" << endl;
                    
                    // manda senial al proceso, activa el handler de la camara en main
                    kill(getpid(), SIGUSR1);
                    supervisor.notify_end(thread_id);
                    last_detection = now;
                    pthread_barrier_wait(&barrier);
                    
                }
            } else if(detected_car && distance > DISTANCE_THRESHOLD_CM){
                detected_car = false;
                cout << "\u274c Vehículo saliendo." << endl;
                supervisor.notify_end(thread_id);
                usleep(500000); // 200ms
            }else if(distance < 0){
                cerr << "\u274c Error: Distancia no válida." << endl;
                supervisor.recovery_thread(thread_id);
            } else {
                cout << "Distancia medida: " << distance << " cm" << endl;
            }
            supervisor.notify_end(thread_id);
            usleep(500000); // 500 ms
        }
    } catch (const exception &e) {
        cerr << "Error: " << e.what() << endl;
        supervisor.recovery_thread(thread_id);
    } 

    gpioTerminate();
}
