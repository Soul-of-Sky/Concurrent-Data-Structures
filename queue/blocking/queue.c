#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>

#include "atomic.h"
#include "queue.h"

struct queue* q_init(unsigned int size) {
    struct queue* q = (struct queue*) malloc(sizeof(struct queue));
    
    q->buffer = (uval_t*) malloc(size * sizeof(uval_t));
    q->cap = size;
    q->used = 0;

    return q;
}

void q_destroy(struct queue* q){
    free(q->buffer);
    free(q);
}

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

void q_push(struct queue* q, uval_t v){
    pthread_mutex_lock(&lock);
    while(!(q->used < q->cap)) {
        pthread_cond_wait(&cond, &lock);
    }
    q->buffer[q->used++] = v;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&lock);
}

void q_pop(struct queue* q, uval_t* v){
    pthread_mutex_lock(&lock);
    while(!(q->used != 0)) {
        pthread_cond_wait(&cond, &lock);
    }
    *v = q->buffer[--(q->used)];
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&lock);
}

int q_front(struct queue* q, uval_t* v){
    if (q->used) {
        *v = q->buffer[q->cap];
        return 1;
    }

    return 0;
}
