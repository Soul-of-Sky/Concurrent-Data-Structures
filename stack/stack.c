#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>

#include "stack.h"
#include "eliminator.h"
#include "atomic.h"

#define BACKOFF
#define BACKOFF_USECS   100

#define ELIMINATION
#define TIMEOUT_USECS   10
#define PREDICT_NUM_THEADS  4

int el_push_cnt = 0;
int el_pop_cnt = 0;

#define STACK_EMPTY ((void*) 0x1234567)

static struct s_node* alloc_node(uval_t v) {
    struct s_node* node = malloc(sizeof(struct s_node));
    node->next = NULL;
    node->v = v;

    return node;
}

extern struct stack* s_create() {
    struct stack* s = malloc(sizeof(struct stack));

    s->head = NULL;

    return s;
}

static void free_node(struct s_node* node) {
    free(node);
}

extern void s_destroy(struct stack* s) {
    struct s_node *pred, *curr;

    curr = s->head;
    while(curr) {
        pred = curr;
        curr = pred->next;
        free_node(pred);
    }

    // printf("el_cnt: %d %d\n", el_pop_cnt, el_pop_cnt);
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

extern void s_push(struct stack* s, uval_t v) {
    struct s_node* node = alloc_node(v);
    uint64_t ex_v;

    while(1) {
        if (try_push(s, node)) {
            return;
        } else {
#ifdef ELIMINATION
            exchang2(&_el, PREDICT_NUM_THEADS, (uint64_t) node, TIMEOUT_USECS, &ex_v);
            if (ex_v == 0) {
                // printf("PUSH exchange succeed\n");
                xadd(&el_push_cnt, 1);
                /* the exchanger will free it for us */
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

extern int s_pop(struct stack* s, uval_t* v) {
    struct s_node* top;
    uint64_t ex_v;

    while(1) {
        top = try_pop(s);
        if (top == STACK_EMPTY) {
            return -ENOENT;
        } else if (top == NULL) {
#ifdef ELIMINATION
            exchang2(&_el, PREDICT_NUM_THEADS, 0UL, TIMEOUT_USECS, &ex_v);
            if (ex_v != 0) {
                *v = ((struct s_node*) ex_v)->v;
                xadd(&el_pop_cnt, 1);
                // printf("POP exchange succeed\n");
                //TODO: free node
                return 0;
            }
#else
#ifdef BACKOFF
            backoff();
#endif
#endif
        } else {
            *v = top->v;
            //TODO: free node
            return 0;
        }
    }
}

extern int s_top(struct stack* s, uval_t* v) {
    struct s_node* top = s->head;
    if (top) {
        *v = top->v;
        return 0;
    }

    return -ENOENT;
}
