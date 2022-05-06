#ifndef BPTREE_H
#define BPTREE_H

#include <stdio.h>
#include <stdint.h>

#include "rwlock.h"
#include "util.h"
#include "list.h"

typedef entry_t bp_kv_t;

#define BP_ADD      100
#define BP_DEL      101
#define BP_READ     102
#define BP_WRITE    103

#define BP_INTER    200
#define BP_LEAF     201

struct page {
    int type;
    int is_head;
    int length;
    rwlock_t lock;
    struct list_head list;

    bp_kv_t kv[0];
};

struct bp {
    int page_size;
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