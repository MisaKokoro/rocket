#ifndef ROCKET_NET_IO_THREAD_H
#define ROCKET_NET_IO_THREAD_H
#include <semaphore.h>
#include "rocket/net/eventloop.h"
namespace rocket {

class IOThread {
public:
    IOThread();

    ~IOThread();

    EventLoop* getEventLoop();

    void start();

    void join();
public:
    static void* Main(void *arg);
private:
    pid_t m_thread_id {-1};
    pthread_t m_thread {0};
    EventLoop* m_event_loop {nullptr};

    sem_t m_init_semaphore;
    sem_t m_start_semaphore;
};


}
#endif