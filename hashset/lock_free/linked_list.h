#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include <stdint.h>
#include <stdio.h>

#include "atomic.h"
#include "util.h"

typedef size_t markable_t;

struct ll_node {
    entry_t e;
    markable_t next;
};

struct ll {
    struct ll_node *head, *tail;
};

#define IS_MARKED(v)        ((v) & 0x1)
#define MARK_NODE(v)        ((v) | 0x1)
#define REMOVE_MARK(v)      ((v) & ~0x1)
#define GET_NODE(v)         ((struct ll_node*) REMOVE_MARK(v))

extern struct ll* ll_init();
extern void ll_destroy(struct ll* ll);
extern int ll_insert(struct ll* ll, ukey_t k, uval_t v);
extern int ll_insert2(struct ll* ll, struct ll_node* node);
extern int ll_lookup(struct ll* ll, ukey_t k, uval_t* v);
extern int ll_remove(struct ll* ll, ukey_t k);
extern int ll_range(struct ll* ll, ukey_t k, unsigned int len, uval_t* v_arr);
extern void ll_print(struct ll* ll);

#ifdef LL_DEBUG
#define ll_debug(fmt, args ...) do{fprintf(stdout, fmt, ##args);}while(0)
#else
#define ll_debug(fmt, args) do{}while(0)
#endif

#endif