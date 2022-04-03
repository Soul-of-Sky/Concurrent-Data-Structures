#ifndef BPTREE_H
#define BPTREE_H

#include <stdio.h>
#include <stdint.h>

#include "rwlock.h"
#include "util.h"

typedef entry_t bp_kv_t;

enum page_type {
    INTER = 0,
    LEAF
};

struct page {
    enum page_type type;
    int length;
    rwlock_t lock;

    bp_kv_t kv[0];
};

struct bp {
    rwlock_t lock;
    struct page* head;
};

extern struct bp* bp_init(unsigned int page_size);
extern void bp_destroy(struct bp* bp);
extern int bp_insert(struct bp* bp, ukey_t k, uval_t v);
extern int bp_lookup(struct bp* bp, ukey_t k, uval_t* v);
extern int bp_remove(struct bp* bp, ukey_t k);
extern int bp_range(struct bp* bp, ukey_t k, unsigned int len, uval_t* v_arr);
extern void bp_print(struct bp* bp);
#endif