#ifndef BPTREE_H
#define BPTREE_H

#include <stdio.h>
#include <stdint.h>

#include "rwlock.h"
#include "util.h"
#include "list.h"

typedef entry_t bp_kv_t;

#define BP_READ     0x100
#define BP_ADD      0x101
#define BP_DEL      0x102

#define BP_INTER    0x200
#define BP_LEAF     0x201

struct page {
    int type;
    int length;
    rwlock_t lock;

    /* leaf use only*/
    struct list_head list;

    bp_kv_t kv[0];
};

struct bp {
    int degree;
    rwlock_t root_lock;
    struct page* root;

    rwlock_t list_lock;
    struct list_head leaf_list;
};

extern struct bp* bp_create(unsigned int degree);
extern void bp_destroy(struct bp* bp);
extern int bp_insert(struct bp* bp, ukey_t k, uval_t v);
extern int bp_lookup(struct bp* bp, ukey_t k, uval_t* v);
extern int bp_remove(struct bp* bp, ukey_t k);
extern int bp_range(struct bp* bp, ukey_t k, unsigned int len, uval_t* v_arr);
extern void bp_print(struct bp* bp);
#endif