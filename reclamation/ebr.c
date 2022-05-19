#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "ebr.h"

#define ACTIVE  0x4
#define GC_FRQ  100

extern struct ebr* ebr_create(free_fun_t _free) {
    struct ebr* ebr = (struct ebr*) malloc(sizeof(struct ebr));
    int i;

    ebr->global_epoch = 0;
    pthread_mutex_init(&ebr->lock, NULL);
    for (i = 0; i < 3; i++) {
        INIT_LIST_HEAD(&ebr->rt_lists[i]);
    }
    for (i = 0; i < MAX_NUM_THREADS; i++) {
        ebr->e_nodes[i].used = 0;
    }
    ebr->_free = _free;
    
    return ebr;
}

static void gc_epoch(struct ebr* ebr, unsigned int epoch) {
    struct list_head* list = &ebr->rt_lists[epoch];
    struct e_rt_node *rt_node, *n;

    list_for_each_entry_safe(rt_node, n, list, list) {
        ebr->_free(rt_node->addr);
        list_del(&rt_node->list);
        free(rt_node);
    }
}

extern void ebr_destroy(struct ebr* ebr) {
    int i;
    for (i = 0; i < 3; i++) {
        gc_epoch(ebr, i);
    }
    free(ebr);
}

extern void ebr_thread_register(struct ebr* ebr, int tid) {
    ebr->e_nodes[tid].used = 1;
}

extern void ebr_thread_unregister(struct ebr* ebr, int tid) {
    ebr->e_nodes[tid].used = 0;
}

extern void ebr_enter(struct ebr* ebr, int tid) {
    unsigned int epoch = ACCESS_ONCE(ebr->global_epoch);

    ebr->e_nodes[tid].local_epoch = epoch | ACTIVE;
    memory_mfence();
}

extern void ebr_exit(struct ebr* ebr, int tid) {
    ebr->e_nodes[tid].local_epoch = 0;
    memory_mfence();
}

extern void ebr_put(struct ebr* ebr, void* addr, int tid) {
    struct e_rt_node* rt_node = (struct e_rt_node*) malloc(sizeof(struct e_rt_node));
    unsigned int epoch;

    rt_node->addr = addr;
    epoch = ACCESS_ONCE(ebr->e_nodes[tid].local_epoch);
    epoch &= (~ACTIVE);

    pthread_mutex_lock(&ebr->lock);
    list_add_tail(&rt_node->list, &ebr->rt_lists[epoch]);
    pthread_mutex_unlock(&ebr->lock);

#ifndef MANUAL_GC
    ebr_try_gc(ebr);
#endif
}

extern void ebr_try_gc(struct ebr* ebr) {
    unsigned int epoch, local_epoch, active;
    int i;

    pthread_mutex_lock(&ebr->lock);
    
    if (rand() % GC_FRQ != 0) {
        pthread_mutex_unlock(&ebr->lock);
        return;
    }

    epoch = ACCESS_ONCE(ebr->global_epoch);
    for (i = 0; i < MAX_NUM_THREADS; i++) {
        if (!ebr->e_nodes[i].used) {
            continue;
        }
        local_epoch = ACCESS_ONCE(ebr->e_nodes[i].local_epoch);
        active = (local_epoch & ACTIVE) == ACTIVE;
        if (active && local_epoch != (epoch | ACTIVE)) {
            gc_epoch(ebr, (epoch + 1) % 3);
            pthread_mutex_unlock(&ebr->lock);
            return;
        }
    }
    
    ebr->global_epoch = (epoch + 1) % 3;
    memory_mfence();

    gc_epoch(ebr, (epoch + 2) % 3);
    
    pthread_mutex_unlock(&ebr->lock);
}
