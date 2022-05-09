#ifndef STACK_H
#define STACK_H

#include "util.h"

struct s_node {
    uval_t v;
    struct s_node* next;
};

struct stack {
    struct s_node* head;
};

extern struct stack* s_init();
extern void s_destroy(struct stack* s);
extern void s_push(struct stack* s, uval_t v);
extern int s_pop(struct stack* s, uval_t* v);
extern int s_top(struct stack* s, uval_t* v);

#endif