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

atomic_bool pending_photo(false);

extern pthread_barrier_t barrier;
extern SharedQueue sharedQueue;

string getCurrentTimestamp() {
    auto now = chrono::system_clock::now();
    auto in_time_t = chrono::system_clock::to_time_t(now);
    stringstream ss;
    ss << put_time(localtime(&in_time_t), "%Y%m%d_%H%M%S");
    return ss.str();
}

void handle_signal_camera(int signal){
    if(signal == SIGUSR1) {
        pending_photo = true;
        cout << "Signal received: " << signal << endl;
    }
}

void threadCamera(ThreadSupervisor& supervisor, std::atomic<bool>& running, int thread_id, VideoCapture cam) {
    Mat frame;
    bool frame_captured = false;
    int count = 0;
    string save_dir = "/home/raspy/str-project/photos/";

    while(running){
        if (pending_photo) {
            try{
                supervisor.notify_start(thread_id);

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
                // usleep(5000000);
                //printf("CAM: %p\n", cam);

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
        usleep(50000); // espera 50 ms
    }

    cam.release();
}
