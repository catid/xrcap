// Copyright (c) 2019 Christopher A. Taylor.  All rights reserved.

#ifdef _WIN32

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pthread
{
    void* ptr;
} pthread_t;

typedef struct pthread_mutex
{
    void* ptr;
    void* locker;
} pthread_mutex_t;

typedef struct pthread_cond
{
    void* ptr;
} pthread_cond_t;

typedef void*(*pthread_func_ptr_t)(void*);

int pthread_create(pthread_t* t, void* x, pthread_func_ptr_t f, void* p);
int pthread_join(pthread_t t, void* x);

void pthread_mutex_init(pthread_mutex_t* m, void* p);
void pthread_mutex_lock(pthread_mutex_t* m);
void pthread_mutex_unlock(pthread_mutex_t* m);
void pthread_mutex_destroy(pthread_mutex_t* m);

void pthread_cond_init(pthread_cond_t* c, void* p);
void pthread_cond_broadcast(pthread_cond_t* c);
void pthread_cond_wait(pthread_cond_t* c, pthread_mutex_t* m);
void pthread_cond_destroy(pthread_cond_t* c);

void sched_yield();

#ifdef __cplusplus
} // extern "C"
#endif

#endif // _WIN32
