#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "qsbr.h"

#define GC_FRQ  100

extern struct qsbr* qsbr_create(free_fun_t _free) {
    struct qsbr* qsbr = (struct qsbr*) malloc(sizeof(struct qsbr));
    struct qs_node* qs_node;
    int i;

    qsbr->global_epoch = 0;
    pthread_mutex_init(&qsbr->lock, NULL);
    for (i = 0; i < MAX_NUM_THREADS; i++) {
        qs_node = &qsbr->qs_nodes[i];
        qs_node->used = 0;
        INIT_LIST_HEAD(&qs_node->rt_list);
    }
    qsbr->_free = _free;

    return qsbr;
}

static void gc_epoch_before(struct qsbr* qsbr, unsigned long epoch) {
    struct list_head* list;
    struct qs_node* qs_node;
    struct rt_node *rt_node, *n;
    int i;

    for (i = 0; i < MAX_NUM_THREADS; i++) {
        qs_node = &qsbr->qs_nodes[i];
        list = &qs_node->rt_list;
        list_for_each_entry_safe(rt_node, n, list, list) {
            if (rt_node->epoch >= epoch) {
                break;
            }
            qsbr->_free(rt_node->addr);
            list_del(&rt_node->list);
            free(rt_node);
        }
    }
}

extern void qsbr_destroy(struct qsbr* qsbr) {
    int i;
    gc_epoch_before(qsbr, UINT64_MAX);
    free(qsbr);
}

extern void qsbr_thread_register(struct qsbr* qsbr, int tid) {
    unsigned long epoch = ACCESS_ONCE(qsbr->global_epoch);
    qsbr->qs_nodes[tid].local_epoch = epoch;
    qsbr->qs_nodes[tid].used = 1;
}

extern void qsbr_thread_exit(struct qsbr* qsbr, int tid) {
    qsbr->qs_nodes[tid].used = 0;
}

extern void qsbr_put(struct qsbr* qsbr, void* addr, int tid) {
    struct rt_node* rt_node = (struct rt_node*) malloc(sizeof(struct rt_node));
    struct qs_node* qs_node = &qsbr->qs_nodes[tid];

    rt_node->addr = addr;
    rt_node->epoch = ACCESS_ONCE(qsbr->qs_nodes[tid].local_epoch);

    list_add_tail(&rt_node->list, &qs_node->rt_list);
    memory_mfence();

#ifndef MANUAL_GC
    qsbr_try_gc(qsbr);
#endif
}

extern void qsbr_checkpoint(struct qsbr* qsbr, int tid) {
    xadd(&qsbr->qs_nodes[tid].local_epoch, 1);
}

extern void qsbr_try_gc(struct qsbr* qsbr) {
    unsigned long epoch, min_epoch = UINT64_MAX;
    int i;

    pthread_mutex_lock(&qsbr->lock);

    if (rand() % GC_FRQ != 0) {
        return;
    }

    for (i = 0; i < MAX_NUM_THREADS; i++) {
        if (!qsbr->qs_nodes[i].used) {
            continue;
        }
        epoch = ACCESS_ONCE(qsbr->qs_nodes[i].local_epoch);
        min_epoch = min_epoch > epoch ? epoch : min_epoch;
    }
    
    gc_epoch_before(qsbr, min_epoch);
    qsbr->global_epoch = epoch;

    pthread_mutex_unlock(&qsbr->lock);
}
