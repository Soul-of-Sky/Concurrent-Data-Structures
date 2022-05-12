#ifndef QSBR_H
#define QSBR_H

#include <pthread.h>

#include "util.h"
#include "list.h"

typedef void (*free_fun_t)(void*);

struct qs_rt_node {
    void* addr;
    unsigned long epoch;
    struct list_head list;
};

struct qs_node {
    int used;
    unsigned long local_epoch;
    struct list_head rt_list;
};

struct qsbr {
    pthread_mutex_t lock;
    unsigned long global_epoch;
    struct qs_node qs_nodes[MAX_NUM_THREADS];
    free_fun_t _free;
};

extern struct qsbr* qsbr_create(free_fun_t _free);
extern void qsbr_destroy(struct qsbr* qsbr);
extern void qsbr_thread_register(struct qsbr* qsbr, int tid);
extern void qsbr_thread_unregister(struct qsbr* qsbr, int tid);
extern void qsbr_checkpoint(struct qsbr* qsbr, int tid);
extern void qsbr_put(struct qsbr* qsbr, void* addr, int tid);
extern void qsbr_try_gc(struct qsbr* qsbr);

#endif