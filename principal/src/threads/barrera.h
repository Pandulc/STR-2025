#ifndef BARRERA_H
#define BARRERA_H

#include "supervisor.h"
#include <atomic>

using namespace std;

void threadBarrier(ThreadSupervisor& supervisor, atomic<bool>& running, int thread_id);

#endif // THREAD_BARRIER_H
