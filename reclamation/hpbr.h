#ifndef HPBR_H
#define HPBR_H

#include <pthread.h>

#include "list.h"
#include "util.h"

#define MAX_NUM_HPS_PER_THREAD  3

typedef void (*free_fun_t)(void*);

struct hp_rt_node {
    void* addr;
    struct list_head list;
};

struct hp_node {
    int used;
    void** hps[MAX_NUM_HPS_PER_THREAD];
    struct list_head rt_list;
};

struct hpbr {
    pthread_mutex_t lock;
    /* used for multi-layer indexes like lock-free skiplist */
    int hp_levels;
    struct hp_node hp_nodes[MAX_NUM_THREADS];
    free_fun_t _free;
};

extern struct hpbr* hpbr_create(free_fun_t _free, int hp_levels);
extern void hpbr_destroy(struct hpbr* hpbr);
extern struct hp_node* hpbr_thread_register(struct hpbr* hpbr, int tid);
extern void hpbr_thread_unregister(struct hpbr* hpbr, int tid);
extern void hpbr_hold(struct hpbr* hpbr, int index, int level, void* addr, int tid);
extern void hpbr_release(struct hpbr* hpbr, int index, int level, int tid);
extern void hpbr_release2(struct hpbr* hpbr, int index, int tid);
extern void hpbr_retire(struct hpbr* hpbr, void* addr, int tid);
extern void hpbr_try_gc(struct hpbr* hpbr);

#endif