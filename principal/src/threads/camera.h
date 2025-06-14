#ifndef CAMERA_H
#define CAMERA_H

#include <opencv2/opencv.hpp>
#include "supervisor.h"
#include <atomic>

using namespace std;
using namespace cv;

void threadCamera(ThreadSupervisor& supervisor, atomic<bool>& running, int thread_id, VideoCapture cam);
//void threadCamera(VideoCapture cam);
void handle_signal_camera(int signal);

#endif //CAMERA_H
