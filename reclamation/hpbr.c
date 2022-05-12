#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "hpbr.h"

#define GC_FRQ  100

extern struct hpbr* hpbr_create(free_fun_t _free, int hp_levels) {
    struct hpbr* hpbr = (struct hpbr*) malloc(sizeof(struct hpbr));
    struct hp_node* hp_node;
    int i, j;

    for (i = 0; i < MAX_NUM_THREADS; i++) {
        hp_node = &hpbr->hp_nodes[i];
        hp_node->used = 0;
        INIT_LIST_HEAD(&hp_node->rt_list);
        pthread_mutex_init(&hp_node->lock, NULL);
        for (j = 0; j < MAX_NUM_HPS_PER_THREAD; j++) {
            hp_node->hps[j] = (void*) calloc(hp_levels, sizeof(void*));
        }
    }
    hpbr->_free = _free;
    pthread_mutex_init(&hpbr->lock, NULL);
    hpbr->hp_levels = hp_levels;

    return hpbr;
}

extern void hpbr_destroy(struct hpbr* hpbr) {
    struct hp_node* hp_node;
    struct hp_rt_node *rt_node, *n;
    struct list_head* list;
    int i, j;

    for (i = 0; i < MAX_NUM_THREADS; i++) {
        hp_node = &hpbr->hp_nodes[i];
        list = &hp_node->rt_list;
        list_for_each_entry_safe(rt_node, n, list, list) {
            hpbr->_free(rt_node->addr);
            list_del(&rt_node->list);
            free(rt_node);
        }

        for (j = 0; j < MAX_NUM_HPS_PER_THREAD; j++) {
            free(hp_node->hps[j]);
        }
    }

    free(hpbr);
}

extern struct hp_node* hpbr_thread_register(struct hpbr* hpbr, int tid) {
    hpbr->hp_nodes[tid].used = 1;
}

extern void hpbr_thread_unregister(struct hpbr* hpbr, int tid) {
    hpbr->hp_nodes[tid].used = 0;
}

extern void hpbr_hold(struct hpbr* hpbr, int index, int level, void* addr, int tid) {
    hpbr->hp_nodes[tid].hps[index][level] = addr;
}

extern void hpbr_release(struct hpbr* hpbr, int index, int level, int tid) {
    hpbr->hp_nodes[tid].hps[index][level] = 0;
}

extern void hpbr_release2(struct hpbr* hpbr, int index, int tid) {
    memset(hpbr->hp_nodes[tid].hps[index], 0, hpbr->hp_levels);
}

extern void hpbr_release_all(struct hpbr* hpbr, int tid) {
    int i;
    
    for (i = 0; i < MAX_NUM_HPS_PER_THREAD; i++) {
        memset(hpbr->hp_nodes[tid].hps[i], 0, hpbr->hp_levels);
    }
}

extern void hpbr_retire(struct hpbr* hpbr, void* addr, int tid) {
    struct hp_node* hp_node = &hpbr->hp_nodes[tid];
    struct list_head* list = &hp_node->rt_list;
    struct hp_rt_node* rt_node = (struct hp_rt_node*) malloc(sizeof(struct hp_rt_node));
    rt_node->addr = addr;

    pthread_mutex_lock(&hp_node->lock);
    list_add_tail(&rt_node->list, list);
    pthread_mutex_unlock(&hp_node->lock);

#ifndef MANUAL_GC
    hpbr_try_gc(hpbr);
#endif
}

extern void hpbr_try_gc(struct hpbr* hpbr) {
    struct list_head *list;
    struct hp_node* hp_node;
    struct hp_rt_node *rt_node, *n;
    const int max_num_hp = MAX_NUM_THREADS * MAX_NUM_HPS_PER_THREAD;
    void* hps[max_num_hp];
    void* hp;
    int hps_len;
    int safe, i, j, k;

    pthread_mutex_lock(&hpbr->lock);
    
    if (rand() % GC_FRQ != 0) {
        pthread_mutex_unlock(&hpbr->lock);
        return;
    }

    for (i = 0; i < MAX_NUM_THREADS; i++) {
        hp_node = &hpbr->hp_nodes[i];
        if (hp_node->used) {
            for (j = 0; j < MAX_NUM_HPS_PER_THREAD; j++) {
                for (k = 0; k < hpbr->hp_levels; k++) {
                    hp = hp_node->hps[j][k];
                    if (hp) {
                        hps[hps_len++] = hp;
                    }
                }
            }
        }
    }
    
    for (i = 0; i < MAX_NUM_THREADS; i++) {
        hp_node = &hpbr->hp_nodes[i];
        list = &hp_node->rt_list;
        safe = 1;
        list_for_each_entry_safe(rt_node, n, list, list) {
            for (j = 0; j < hps_len; j++) {
                if (hps[j] == rt_node->addr) {
                    safe = 0;
                    break;
                }
            }
            if (safe) {
                hpbr->_free(rt_node->addr);
                pthread_mutex_lock(&hp_node->lock);
                list_del(&rt_node->list);
                pthread_mutex_unlock(&hp_node->lock);
                free(rt_node);
            }
        }
    }

    pthread_mutex_unlock(&hpbr->lock);
}
