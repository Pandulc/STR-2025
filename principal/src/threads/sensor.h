#ifndef SENSOR_H
#define SENSOR_H
#include "supervisor.h"
#include <atomic>

using namespace std;

void threadSensor(ThreadSupervisor& supervisor, atomic<bool>& running, int thread_id);

#endif