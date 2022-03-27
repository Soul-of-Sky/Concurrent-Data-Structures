#ifndef QUEUE_H
#define QUEUE_H

#include <stdio.h>
#include <stdint.h>

typedef size_t qval_t;

struct q_node {
    qval_t v;
    struct q_node* next;
}

struct queue {
    struct q_node *head, *tail;
}

extern struct queue* q_init();
extern void q_destroy(struct queue* q);
extern void q_push(struct queue* q, size_t v);
extern qval_t q_pop(struct queue* q);
extern qval_t q_top(struct queue* q);

#endif