#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "hpbr.h"

#define GC_FRQ  100

extern struct hpbr* hpbr_create(free_fun_t _free) {
    struct hpbr* hpbr = (struct hpbr*) malloc(sizeof(struct hpbr));
    struct hp_node* hp_node;
    int i;

    for (i = 0; i < MAX_NUM_THREADS; i++) {
        hp_node = &hpbr->hp_nodes[i];
        hp_node->used = 0;
        INIT_LIST_HEAD(&hp_node->rt_list);
        memset(hp_node->hps, 0, MAX_NUM_HP_PER_THREAD * sizeof(void*));
    }
    hpbr->_free = _free;
    pthread_mutex_init(&hpbr->lock, NULL);

    return hpbr;
}

extern struct hp_node* hpbr_register_thread(struct hpbr* hpbr, int tid) {
    hpbr->hp_nodes[tid].used = 1;
}

extern void hp_thread_exit(struct hpbr* hpbr, int tid) {
    hpbr->hp_nodes[tid].used = 0;
}

extern void hp_hold(struct hpbr* hpbr, int level, void* addr, int tid) {
    hpbr->hp_nodes[tid].hps[level] = addr;
}

extern void hp_release(struct hpbr* hpbr, int level, int tid) {
    hpbr->hp_nodes[tid].hps[level] = 0;
}

extern void hp_retire(struct hpbr* hpbr, void* addr, int tid) {
    struct hp_node* hp_node = &hpbr->hp_nodes[tid];
    struct list_head* list = &hp_node->rt_list;
    struct rt_node* rt_node = (struct rt_node*) malloc(sizeof(struct rt_node));
    rt_node->addr = addr;

    list_add_tail(&rt_node->list, list);

#ifndef MANUAL_GC
    hp_try_gc(hpbr);
#endif
}

extern void hp_try_gc(struct hpbr* hpbr) {
    struct list_head *list;
    struct hp_node* hp_node;
    struct rt_node* rt_node;
    const int max_num_hp = MAX_NUM_THREADS * MAX_NUM_HP_PER_THREAD;
    void* hps[max_num_hp];
    void* hp;
    int hps_len;
    int safe, i, j;

    if (rand() % GC_FRQ != 0) {
        return;
    }

    pthread_mutex_lock(&hpbr->lock);

    for (i = 0; i < MAX_NUM_THREADS; i++) {
        hp_node = &hpbr->hp_nodes[i];
        if (hp_node->used) {
            for (j = 0; j < MAX_NUM_HP_PER_THREAD; j++) {
                hp = hp_node->hps[j];
                if (hp) {
                    hps[hps_len++] = hp;
                }
            }
        }
    }
    
    for (i = 0; i < MAX_NUM_THREADS; i++) {
        list = &hp_node->rt_list;
        safe = 1;
        list_for_each_entry(rt_node, list, list) {
            for (j = 0; j < hps_len; j++) {
                if (hps[j] == rt_node->addr) {
                    safe = 0;
                    break;
                }
            }
            if (safe) {
                hpbr->_free(rt_node->addr);
                list_del(&rt_node->list);
                free(rt_node);
            }
        }
    }
    pthread_mutex_unlock(&hpbr->lock);
}
