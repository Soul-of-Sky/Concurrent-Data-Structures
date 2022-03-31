#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>

#include "atomic.h"
#include "queue.h"

struct queue* q_init(unsigned int size) {
    struct queue* q = (struct queue*) malloc(sizeof(struct queue));
    
    q->buffer = (qval_t*) malloc(size * sizeof(qval_t));
    q->cap = size;
    q->head = 0;
    q->tail = 0;

    pthread_mutex_init(&q->push_mutex, NULL);
    pthread_cond_init(&q->not_)
}

