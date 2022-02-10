#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include <stdint.h>

#include "spinlock.h"

typedef uint64_t pkey_t;
typedef uint64_t pval_t;

typedef struct entry
{
    pkey_t key;
    pval_t value;
} entry_t;

struct ll_node
{
    entry_t e;
    spinlock_t lock;
    struct ll_node *next;
};

struct linked_list
{
    struct ll_node *head, *tail;
};

#define node_key(node) ((node)->e.key)
#define node_val(node) ((node)->e.value)

extern struct linked_list *ll_init();
extern int ll_insert(struct linked_list *ll, pkey_t key, pval_t val);
extern int ll_update(struct linked_list *ll, pkey_t key, pval_t val);
extern int ll_lookup(struct linked_list *ll, pkey_t key);
extern int ll_remove(struct linked_list *ll, pkey_t key);
extern int ll_range(struct linked_list *ll, pkey_t low, pkey_t high, entry_t *res_arr);
extern void ll_print(struct linked_list *ll);
extern void ll_destory(struct linked_list *ll);

#endif //LINKED_LIST_H