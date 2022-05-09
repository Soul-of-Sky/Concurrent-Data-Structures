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
    
    /* crabbing lock will use these*/
    int locked;
    struct page* fa;

    /* leaf use only*/
    struct page* next;

    bp_kv_t kv[0];
};

struct bp {
    int degree;
    rwlock_t lock;
    struct page* root;
    
    int locked;
};

extern struct bp* bp_init(unsigned int degree);
extern void bp_destroy(struct bp* bp);
extern int bp_insert(struct bp* bp, ukey_t k, uval_t v);
extern int bp_lookup(struct bp* bp, ukey_t k, uval_t* v);
extern int bp_remove(struct bp* bp, ukey_t k);
extern int bp_range(struct bp* bp, ukey_t k, unsigned int len, uval_t* v_arr);
extern void bp_print(struct bp* bp);
#endif