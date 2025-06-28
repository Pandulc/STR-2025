/**
 * @file camera.cpp
 * @brief Módulo de captura de imágenes mediante cámara USB, activado por señal y sincronizado con otros hilos.
 */

#include "camera.h"
#include <atomic>
#include <iostream>
#include <csignal>
#include <unistd.h>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <filesystem> 
#include <opencv2/opencv.hpp>
#include "supervisor.h"
#include "shared_data.h"

using namespace std;
using namespace cv;

// Variable atomica para controlar si hay una foto pendiente
atomic_bool pending_photo(false);

extern pthread_barrier_t barrier;
extern SharedQueue sharedQueue;

/**
 * @brief Obtiene la marca de tiempo actual en formato YYYYMMDD_HHMMSS.
 * @return Cadena con la marca de tiempo actual.
 */
string getCurrentTimestamp() {
    auto now = chrono::system_clock::now();
    auto in_time_t = chrono::system_clock::to_time_t(now);
    stringstream ss;
    ss << put_time(localtime(&in_time_t), "%Y%m%d_%H%M%S");
    return ss.str();
}

/**
 * @brief Manejador de señal para activar la captura de imagen.
 * @param signal Numero de señal recibida (espera SIGUSR1).
 */
void handle_signal_camera(int signal){
    if(signal == SIGUSR1) {
        pending_photo = true;
        cout << "Signal received: " << signal << endl;
    }
}

/**
 * @brief Hilo de ejecucion encargado de capturar una imagen cuando se recibe una señal.
 *
 * Este hilo espera a que se active la bandera `pending_photo` para iniciar la captura de imagen.
 * Realiza multiples intentos hasta obtener un frame valido, guarda la imagen capturada con
 * marca de tiempo en el nombre, y la coloca en una cola compartida para su posterior envio.
 * Utiliza una barrera para sincronizar el ciclo con el resto del sistema.
 *
 * @param supervisor Referencia al supervisor de hilos para control de fallos y monitoreo.
 * @param running Bandera atomica que indica si el hilo debe continuar en ejecucion.
 * @param thread_id Identificador del hilo para la supervision.
 * @param cam Objeto VideoCapture abierto con la camara correspondiente.
 */
void threadCamera(ThreadSupervisor& supervisor, std::atomic<bool>& running, int thread_id, VideoCapture cam) {
    Mat frame;
    bool frame_captured = false;
    int count = 0;
    string save_dir = "/home/raspy/str-project/photos/";

    while(running){
        if (pending_photo) {
            try{
                supervisor.notify_start(thread_id);

                // Intenta capturar multiples frames hasta obtener uno valido
                for (int i = 0; i < 40; i++) {
                    cam >> frame;
                    if (!frame.empty()) {
                        if (count >= 3){
                            frame_captured = true;
                            count = 0;
                            break;
                        }
                        count++;
                    }
                    usleep(10000); // Espera 10ms entre intentos
                }

                if (frame_captured) {
                    string filename = save_dir + "foto_" + getCurrentTimestamp() + ".jpg";
                    
                    if (imwrite(filename, frame)) {
                        cout << "Foto guardada como: " << filename << endl;
                        sharedQueue.push(filename);
                        pending_photo = false;
                        supervisor.notify_end(thread_id);
                        frame_captured = false;
                        count = 0;
                        pthread_barrier_wait(&barrier);
                    } else {
                        cerr << "\u274c Error al guardar la foto." << endl;
                        supervisor.recovery_thread(thread_id);
                    }
                } else {
                    cerr << "\u274c Error al capturar la imagen." << endl;
                    supervisor.recovery_thread(thread_id);
                }
            } catch (const exception &e) {
                cerr << "Error: " << e.what() << endl;
                supervisor.recovery_thread(thread_id);
            }
        }
        usleep(50000); // Espera 50 ms antes de volver a verificar
    }

    cam.release();
}
