#ifndef COMMUNICATOR_H
#define COMMUNICATOR_H
#include "supervisor.h"
#include <atomic>

using namespace std;

void threadCommunicator(ThreadSupervisor& supervisor, atomic<bool>& running, int thread_id);

#endif