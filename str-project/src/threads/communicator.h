#ifndef COMMUNICATOR_H
#define COMMUNICATOR_H
#include "supervisor.h"

//void threadSensor(ThreadSupervisor& , int);
void threadCommunicator(ThreadSupervisor& supervisor, int thread_id);

#endif