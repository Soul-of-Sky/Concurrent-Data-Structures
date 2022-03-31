#ifndef SKIPLIST_H
#define SKIPLIST_H

#include <stdint.h>
#include <stdio.h>

#include "spinlock.h"

typedef uint64_t skey_t;
typedef char*    sval_t;

typedef struct {
    skey_t k;
    sval_t v;
}entry_t;

typedef size_t markable_t;

struct sl_node {
    entry_t e;
    spinlock_t lock;
    markable_t next[0];
};

struct sl {
    struct sl_node *head, *tail;
    int max_levels;
    int levels;
};

#define IS_MARKED(v)        ((v) & 0x1)
#define MARK_NODE(v)        ((v) | 0x1)
#define REMOVE_MARK(v)      ((v) & ~0x1)

#define IS_FULLY_LINKED(v)  ((v) & 0x2)
#define FULLY_LINK(v)       ((v) | 0x2)
#define UNLINK(v)           ((v) & ~0x2)

#define GET_NODE(v)         ((struct ll_node*) REMOVE_MARK(v))

extern struct sl* sl_init(int max_levels);
extern void sl_destroy(struct sl* sl);
extern int sl_insert(struct sl* sl, skey_t k, sval_t v);
extern int sl_lookup(struct sl* sl, skey_t k, sval_t* v);
extern int sl_remove(struct sl* sl, skey_t k);
extern int sl_range(struct sl* sl, skey_t k, unsigned int len, sval_t* v_arr);
extern void ll_print(struct sl* sl);

#ifdef SL_DEBUG
#define sl_debug(fmt, args ...) do{fprintf(stdout, fmt, ##args);}while(0)
#else
#define sl_debug(fmt, args) do{}while(0)
#endif

#endif