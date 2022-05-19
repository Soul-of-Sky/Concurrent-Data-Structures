#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>

#include "queue.h"
#include "atomic.h"

static struct q_node* alloc_node(uval_t v) {
    struct q_node* node = (struct q_node*) malloc(sizeof(struct q_node));

    node->next = NULL;
    node->v = v;

    return node;
}

static void free_node(struct q_node* node) {
    free(node);
}

struct queue* q_create() {
    struct queue* q = (struct queue*) malloc(sizeof(struct queue));
    struct q_node* node = alloc_node(0);
    
    q->head = node;
    q->tail = node;

    q->ebr = ebr_create((free_fun_t) free_node);

    return q;
}

void q_destroy(struct queue* q) {
    struct q_node *pred, *curr;

    pred = q->head;

    while(pred) {
        curr = pred->next;
        free_node(pred);
        pred = curr;
    }

    ebr_destroy(q->ebr);

    free(q);
}

void q_push(struct queue* q, uval_t v, int tid) {
    struct q_node* node = alloc_node(v);
    struct q_node *last, *next;

    ebr_enter(q->ebr, tid);

    while(1) {
        last = q->tail;
        next = last->next;
        if (last == q->tail) {
            if (next == NULL) {
                if (cmpxchg2(&last->next, next, node)) {
                    cmpxchg2(&q->tail, last, node);
                    ebr_exit(q->ebr, tid);
                    return;
                }
            } else {
                cmpxchg2(&q->tail, last, next);
            }
        }
    }
}

int q_pop(struct queue* q, uval_t* v, int tid) {
    struct q_node *first, *last, *next;

    ebr_enter(q->ebr, tid);

    *v = 0;
    while(1) {
        last = q->tail;
        first = q->head;
        next = first->next;
        if (first == q->head) {
            if (first == last) {
                if (next == NULL) {
                    ebr_exit(q->ebr, tid);
                    return -ENOENT;
                }
                cmpxchg2(&q->tail, last, next);
            } else {
                *v = next->v;
                if (cmpxchg2(&q->head, first, next)) {
                    ebr_put(q->ebr, first, tid);
                    ebr_exit(q->ebr, tid);
                    return 0;
                }
            }
        }
    }
}

int q_front(struct queue* q, uval_t* v, int tid) {
    struct q_node *first, *last, *next;

    ebr_enter(q->ebr, tid);

    *v = 0;
    while(1) {
        last = q->tail;
        first = q->head;
        next = first->next;
        if (first == q->head) {
            if (first == last) {
                if (next == NULL) {
                    ebr_exit(q->ebr, tid);
                    return -ENOENT;
                }
                cmpxchg2(&q->tail, last, next);
            } else {
                *v = next->v;
                ebr_exit(q->ebr, tid);
                return 0;
            }
        }
    }
}