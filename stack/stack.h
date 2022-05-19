#ifndef STACK_H
#define STACK_H

#include "util.h"
#include "ebr.h"

// #define BACKOFF
#define BACKOFF_USECS   100

// #define ELIMINATION
#define TIMEOUT_USECS   100
#define PREDICT_NUM_THEADS  8

struct s_node {
    uval_t v;
    struct s_node* next;
};

struct stack {
    struct s_node* head;
    struct ebr* ebr;
#ifdef ELIMINATION
    struct elimination* el;
#endif
};

extern struct stack* s_create();
extern void s_destroy(struct stack* s);
extern void s_push(struct stack* s, uval_t v, int tid);
extern int s_pop(struct stack* s, uval_t* v, int tid);
extern int s_top(struct stack* s, uval_t* v, int tid);

#endif