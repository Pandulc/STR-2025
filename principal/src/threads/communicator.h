#ifndef COMMUNICATOR_H
#define COMMUNICATOR_H
#include "supervisor.h"
#include <atomic>

using namespace std;

//void threadSensor(ThreadSupervisor& , int);
void threadCommunicator(ThreadSupervisor& supervisor, atomic<bool>& running, int thread_id);

#endif