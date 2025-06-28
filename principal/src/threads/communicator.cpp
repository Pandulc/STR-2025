#include "../shared_data.h"
#include <curl/curl.h>  // for HTTP POST
#include "supervisor.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <atomic>

using namespace std;

/** Barrera para sincronizar hilos luego de cada envío */
extern pthread_barrier_t barrier;

/** Cola compartida que contiene las rutas de imágenes */
extern SharedQueue sharedQueue;

/** Bandera para indicar si se debe levantar la barrera tras una respuesta válida */
extern atomic<bool> lift_barrier;

/**
 * Se encarga de concatenar el contenido recibido en una cadena de texto.
 *
 * @param contents Puntero a los datos recibidos.
 * @param size Tamaño de cada bloque de datos.
 * @param nmemb Cantidad de bloques.
 * @param userp Puntero a la variable donde se almacenará la respuesta completa.
 * @return Número total de bytes procesados.
 */
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    std::string* response = static_cast<std::string*>(userp);
    response->append(static_cast<char*>(contents), realsize);
    return realsize;
}

/**
 * Hilo que envía imágenes al servidor y sincroniza por barrera
 * @param supervisor Referencia al supervisor de hilos
 * @param running Bandera global para detener ejecución
 * @param thread_id ID del hilo actual
 */
void threadCommunicator(ThreadSupervisor& supervisor, std::atomic<bool>& running, int thread_id) {
    curl_global_init(CURL_GLOBAL_ALL);

    while (running) {
        try {
            string photo_path = sharedQueue.wait_and_pop();
            supervisor.notify_start(thread_id);
            cout << "Procesando foto: " << photo_path << endl;

            // Verificar existencia del archivo
            if(access(photo_path.c_str(), F_OK) == -1) {
                cerr << "Error: Archivo no existe - " << photo_path << endl;
                supervisor.recovery_thread(thread_id);
                continue;
            }

            CURL* curl = curl_easy_init();
            if (!curl) {
                cerr << "Error al inicializar cURL" << endl;
                supervisor.recovery_thread(thread_id);
                continue;
            }

            string respuesta;
            long http_code = 0;

            // Configuración segura de cURL
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &respuesta);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

            // Configurar formulario multipart
            curl_mime* form = curl_mime_init(curl);
            curl_mimepart* field = curl_mime_addpart(form);
            curl_mime_name(field, "imagen");
            curl_mime_filedata(field, photo_path.c_str());

            curl_easy_setopt(curl, CURLOPT_URL, "http://192.168.0.103:5000/procesar");
            curl_easy_setopt(curl, CURLOPT_MIMEPOST, form);

            CURLcode res = curl_easy_perform(curl);

            if (res != CURLE_OK) {
                cerr << "Error en cURL: " << curl_easy_strerror(res) << endl;
            } else {
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
                cout << "Respuesta HTTP: " << http_code << endl;
                
                if (http_code == 200) {
                    cout << "Contenido: " << respuesta << endl;
                    lift_barrier.store(respuesta.find("true") != string::npos);
                }
            }

            // Limpieza segura
            curl_mime_free(form);
            curl_easy_cleanup(curl);
            supervisor.notify_end(thread_id);
            pthread_barrier_wait(&barrier);

        } catch (const exception& e) {
            cerr << "Excepción en comunicador: " << e.what() << endl;
            supervisor.recovery_thread(thread_id);
        }
    }
    curl_global_cleanup();
}
