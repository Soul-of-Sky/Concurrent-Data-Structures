#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>

#include "queue.h"
#include "atomic.h"

static struct q_node* alloc_node(qval_t v) {
    struct q_node* node = (struct q_node*) malloc(sizeof(struct q_node));

    node->next = NULL;
    node->v = v;

    return node;
}

struct queue* q_init() {
    struct queue* q = (struct queue*) malloc(sizeof(struct queue));
    struct q_node* node = alloc_node(NULL);
    
    q->head = node;
    q->tail = node;

    return q;
}

static void free_node(struct q_node* node) {
    free(node);
}

void q_destroy(struct queue* q) {
    struct q_node *pred, *curr;

    pred = q->head;

    while(pred) {
        curr = pred->next;
        free_node(pred);
        pred = curr;
    }

    free(q);
}

void q_push(struct queue* q, qval_t v) {
    struct q_node* node = alloc_node(v);
    struct q_node *last, *next;

    while(1) {
        last = q->tail;
        next = last->next;
        if (last == q->tail) {
            if (next == NULL) {
                if (cmpxchg2(&last->next, next, node)) {
                    cmpxchg2(&q->tail, last, node);
                    return;
                }
            } else {
                cmpxchg2(&q->tail, last, next);
            }
        }
    }
}

qval_t q_pop(struct queue* q) {
    struct q_node *first, *last, *next;

    while(1) {
        last = q->tail;
        first = q->head;
        next = first->next;
        if (first == q->head) {
            if (first == last) {
                if (next == NULL) {
                    return NULL;
                }
                cmpxchg2(&q->tail, last, next);
            } else {
                if (cmpxchg2(&q->head, first, next)) {
                    return next->v;
                }
            }
        }
    }
}

qval_t q_top(struct queue* q) {
    struct q_node *first = q->head;

    return first->v;
}