#ifndef SKIPLIST_H
#define SKIPLIST_H

#include <stdint.h>
#include <stdio.h>

#include "spinlock.h"
#include "util.h"

typedef size_t markable_t;

struct sl_node {
    entry_t e;
    int levels;
    spinlock_t lock;
    markable_t next[0];
};

struct sl {
    struct sl_node *head, *tail;
    int max_levels;
    volatile int levels;
};

#define IS_TAGED(v, t)      ((unsigned long) (v) & t)
#define ADD_TAG(v, t)       ((unsigned long) (v) | t)
#define DEL_TAG(v, t)       ((unsigned long) (v) & ~t)

#define GET_SIGN(n)         (((struct sl_node*)n)->next[0])

#define IS_MARKED(n)        IS_TAGED(GET_SIGN(n), 0x1)
#define MARK_NODE(n)        ADD_TAG(GET_SIGN(n), 0x1)
#define REMOVE_MARK(n)      DEL_TAG(GET_SIGN(n), 0x1)

#define IS_FULLY_LINKED(n)  IS_TAGED(GET_SIGN(n), 0x2)
#define FULLY_LINK(n)       ADD_TAG(GET_SIGN(n), 0x2)
#define FULLY_LINK2(v)      ADD_TAG(v, 0x2)
#define UNLINK(n)           DEL_TAG(GET_SIGN(n), 0x2)

#define GET_NODE(v)         ((struct ll_node*) DEL_TAG(v, 0x3))

extern struct sl* sl_init(int max_levels);
extern void sl_destroy(struct sl* sl);
extern int sl_insert(struct sl* sl, ukey_t k, uval_t v);
extern int sl_lookup(struct sl* sl, ukey_t k, uval_t* v);
extern int sl_remove(struct sl* sl, ukey_t k);
extern int sl_range(struct sl* sl, ukey_t k, unsigned int len, uval_t* v_arr);
extern void ll_print(struct sl* sl);

#ifdef SL_DEBUG
#define sl_debug(fmt, args ...) do{fprintf(stdout, fmt, ##args);}while(0)
#else
#define sl_debug(fmt, args) do{}while(0)
#endif

#endif