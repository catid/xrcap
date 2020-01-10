// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#ifdef _WIN32

#include "PthreadWin32Adapter.h"

#include <windows.h>
#include <mutex>
#include <thread>
#include <condition_variable>

#ifdef __cplusplus
extern "C" {
#endif

int pthread_create(pthread_t* t, void* x, pthread_func_ptr_t f, void* p)
{
    t->ptr = new std::thread([f, p]() {
        f(p);
    });
    return 0;
}

int pthread_join(pthread_t t, void* x)
{
    (void)x;
    std::thread* th = (std::thread*)t.ptr;
    th->join();
    delete th;
    return 0;
}

void pthread_mutex_init(pthread_mutex_t* m, void* p)
{
    m->ptr = new std::mutex;
    m->locker = nullptr;
}

void pthread_mutex_lock(pthread_mutex_t* m)
{
    std::mutex* mtx = (std::mutex*)m->ptr;
    m->locker = new std::unique_lock<std::mutex>(*mtx);
}

void pthread_mutex_unlock(pthread_mutex_t* m)
{
    std::unique_lock<std::mutex>* locker = (std::unique_lock<std::mutex>*)m->locker;
    if (locker) {
        delete locker;
        m->locker = nullptr;
    }
}

void pthread_mutex_destroy(pthread_mutex_t* m)
{
    std::mutex* mtx = (std::mutex*)m->ptr;
    if (mtx) {
        delete mtx;
        m->locker = nullptr;
        m->ptr = nullptr;
    }
}

void pthread_cond_init(pthread_cond_t* c, void* p)
{
    (void)p;
    c->ptr = new std::condition_variable;
}

void pthread_cond_broadcast(pthread_cond_t* c)
{
    std::condition_variable* cond = (std::condition_variable*)c->ptr;
    cond->notify_all();
}

void pthread_cond_wait(pthread_cond_t* c, pthread_mutex_t* m)
{
    std::condition_variable* cond = (std::condition_variable*)c->ptr;
    std::unique_lock<std::mutex>* locker = (std::unique_lock<std::mutex>*)m->locker;
    cond->wait(*locker);
}

void pthread_cond_destroy(pthread_cond_t* c)
{
    std::condition_variable* cond = (std::condition_variable*)c->ptr;
    if (cond) {
        delete cond;
    }
    c->ptr = nullptr;
}

void sched_yield()
{
    ::Sleep(0);
}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // _WIN32
