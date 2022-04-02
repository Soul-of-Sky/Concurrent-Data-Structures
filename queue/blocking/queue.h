#ifndef QUEUE_H
#define QUEUE_H

#include <stdio.h>
#include <stdint.h>
#include <pthread.h>

#include "atomic.h"
#include "util.h"

struct queue {
    uval_t* buffer;
    unsigned int cap;
    int head, tail;
    pthread_mutex_t push_mutex, pop_mutex;
    pthread_cond_t not_full_cond, not_empty_cond;
}

extern struct queue* q_init(unsigned int size);
extern void q_destroy(struct queue* q);
extern void q_push(struct queue* q, uval_t v);
extern int q_pop(struct queue* q, uval_t* v);
extern int q_front(struct queue* q, uval_t* v);

#endif