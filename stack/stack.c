#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>

#include "stack.h"
#include "eliminator.h"
#include "atomic.h"

#define STACK_EMPTY ((void*) 0x1234567)

static struct s_node* alloc_node(uval_t v) {
    struct s_node* node = malloc(sizeof(struct s_node));
    node->next = NULL;
    node->v = v;

    return node;
}

static void free_node(struct s_node* node) {
    free(node);
}

extern struct stack* s_create() {
    struct stack* s = malloc(sizeof(struct stack));

    s->head = NULL;
#ifdef ELIMINATION
    s->el = el_create();
#endif

    s->ebr = ebr_create((free_fun_t)free_node);

    return s;
}

extern void s_destroy(struct stack* s) {
    struct s_node *pred, *curr;

    curr = s->head;
    while(curr) {
        pred = curr;
        curr = pred->next;
        free_node(pred);
    }
#ifdef ELIMINATION
    el_destroy(s->el);
#endif

    ebr_destroy(s->ebr);

    free(s);
}

static int try_push(struct stack* s, struct s_node* node) {
    struct s_node* old_top;

    old_top = s->head;
    node->next = old_top;
    return cmpxchg2(&s->head, old_top, node);
}

/* backoff can alleviate contention in a high contention situation */
static inline void backoff() {
    usleep(BACKOFF_USECS);
}

extern void s_push(struct stack* s, uval_t v, int tid) {
    struct s_node* node = alloc_node(v);
    uint64_t ex_v;

    ebr_enter(s->ebr, tid);
    
    while(1) {
        if (try_push(s, node)) {
            ebr_exit(s->ebr, tid);
            return;
        } else {
#ifdef ELIMINATION
            exchang2(s->el, PREDICT_NUM_THEADS, (uint64_t) node, TIMEOUT_USECS, &ex_v);
            if (ex_v == 0) {
                /* the exchanger will free it for us */
                ebr_exit(s->ebr, tid);
                return;
            }
#else
#ifdef BACKOFF
            backoff();
#endif
#endif
        }
    }
}

static struct s_node* try_pop(struct stack* s) {
    struct s_node *old_top, *new_top; 
    
    old_top = s->head;
    if (old_top == NULL) {
        return STACK_EMPTY;
    }

    new_top = old_top->next;
    if (cmpxchg2(&s->head, old_top, new_top)) {
        return old_top;
    }

    return NULL;
}

extern int s_pop(struct stack* s, uval_t* v, int tid) {
    struct s_node* top;
    uint64_t ex_v;

    ebr_enter(s->ebr, tid);

    while(1) {
        top = try_pop(s);
        if (top == STACK_EMPTY) {
            return -ENOENT;
        } else if (top == NULL) {
#ifdef ELIMINATION
            exchang2(s->el, PREDICT_NUM_THEADS, 0UL, TIMEOUT_USECS, &ex_v);
            if (ex_v != 0) {
                /* exchange succeed */
                *v = ((struct s_node*) ex_v)->v;
                free_node((struct s_node*) ex_v);
                ebr_exit(s->ebr, tid);

                return 0;
            }
#else
#ifdef BACKOFF
            backoff();
#endif
#endif
        } else {
            *v = top->v;
            ebr_put(s->ebr, top, tid);
            ebr_exit(s->ebr, tid);
            return 0;
        }
    }
}

extern int s_top(struct stack* s, uval_t* v, int tid) {
    struct s_node* top = s->head;

    ebr_enter(s->ebr, tid);

    if (top) {
        *v = top->v;
        return 0;
    }

    ebr_exit(s->ebr, tid);

    return -ENOENT;
}
