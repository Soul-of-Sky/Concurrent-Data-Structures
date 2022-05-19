#ifndef QUEUE_H
#define QUEUE_H

#include <stdio.h>
#include <stdint.h>

#include "util.h"
#include "ebr.h"

struct q_node {
    uval_t v;
    struct q_node* next;
};

struct queue {
    struct q_node *head, *tail;
    struct ebr* ebr;
};

extern struct queue* q_create();
extern void q_destroy(struct queue* q);
extern void q_push(struct queue* q, uval_t v, int tid);
extern int q_pop(struct queue* q, uval_t* v, int tid);
extern int q_front(struct queue* q, uval_t* v, int tid);

#endif