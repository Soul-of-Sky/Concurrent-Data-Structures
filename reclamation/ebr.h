#ifndef EBR_H
#define EBR_H

#include <pthread.h>

#include "util.h"
#include "list.h"

typedef void (*free_fun_t)(void*);

struct e_node {
    int used;
    unsigned int local_epoch;
};

struct e_rt_node {
    void* addr;
    struct list_head list;
};

struct ebr {
    pthread_mutex_t lock;
    unsigned int global_epoch;
    struct e_node e_nodes[MAX_NUM_THREADS];
    struct list_head rt_lists[3];
    free_fun_t _free;
};

extern struct ebr* ebr_create(free_fun_t _free);
extern void ebr_destroy(struct ebr* ebr);
extern void ebr_thread_register(struct ebr* ebr, int tid);
extern void ebr_thread_unregister(struct ebr* ebr, int tid);
extern void ebr_enter(struct ebr* ebr, int tid);
extern void ebr_exit(struct ebr* ebr, int tid);
extern void ebr_put(struct ebr* ebr, void* addr, int tid);
extern void ebr_try_gc(struct ebr* ebr);

#endif