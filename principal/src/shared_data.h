#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <queue>
#include <string>
#include <mutex>
#include <condition_variable>
using namespace std;

/**
 * Bandera global atómica que indica si se debe levantar la barrera.
 * Puede ser modificada por distintos hilos para coordinar el acceso.
 */
inline atomic<bool> lift_barrier(false);

/**
 * Clase SharedQueue
 * Cola segura para múltiples hilos, utilizada para almacenar rutas de fotos.
 * Proporciona mecanismos de sincronización usando mutex y condition_variable.
 */
class SharedQueue {
private:
    queue<string> photo_paths;        // Cola de rutas de archivos (ej. imágenes capturadas)
    mutex mtx;                        // Mutex para acceso exclusivo
    condition_variable cv;           // Variable de condición para notificación entre hilos

public:
    /**
     * Inserta una nueva ruta de foto en la cola y notifica a los hilos en espera.
     * @param path Ruta al archivo (ej. imagen)
     */
    void push(const string& path) {
        lock_guard<mutex> lock(mtx);
        photo_paths.push(path);
        cv.notify_one();
    }

    /**
     * Espera hasta que haya un elemento en la cola y lo devuelve.
     * Bloquea el hilo si la cola está vacía hasta que se inserte un nuevo valor.
     * @return La ruta de foto al frente de la cola.
     */
    string wait_and_pop() {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [this] { return !photo_paths.empty(); });
        string path = photo_paths.front();
        photo_paths.pop();
        return path;
    }
};

#endif
