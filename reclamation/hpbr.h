#ifndef HPBR_H
#define HPBR_H

#include <pthread.h>

#include "list.h"
#include "util.h"

#define MAX_NUM_HP_PER_THREAD   30

typedef void (*free_fun_t)(void*);

struct rt_node {
    void* addr;
    struct list_head list;
};

struct hp_node {
    int used;
    void* hps[MAX_NUM_HP_PER_THREAD];
    struct list_head rt_list;
};

struct hpbr {
    pthread_mutex_t lock;
    struct hp_node hp_nodes[MAX_NUM_THREADS];
    free_fun_t _free;
};

extern struct hpbr* hpbr_create(free_fun_t _free);
extern struct hp_node* hpbr_register_thread(struct hpbr* hpbr, int tid);
extern void hp_thread_exit(struct hpbr* hpbr, int tid);
extern void hp_hold(struct hpbr* hpbr, int level, void* addr, int tid);
extern void hp_release(struct hpbr* hpbr, int level, int tid);
extern void hp_retire(struct hpbr* hpbr, void* addr, int tid);
extern void hp_try_gc(struct hpbr* hpbr);

#endif