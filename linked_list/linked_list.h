#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include <stdint.h>

#include "spinlock.h"

typedef uint64_t pkey_t;
typedef uint64_t pval_t;

typedef struct entry {
    pkey_t key;
    pval_t value;
}entry_t;

struct ll_node {
    entry_t e;
    spinlock_t lock;
    struct ll_node* next;
};

struct hp_item;

struct linked_list {
    struct ll_node *head, *tail;
    int hp_count;
    struct hp_item** hp;
};

#define node_key(node) ((node)->e.k)
#define node_val(node) ((node)->e.v)

extern struct linked_list* ll_init();
extern int ll_insert(struct linked_list* ll, pkey_t key, pval_t val);
extern int ll_update(struct linked_list* ll, pkey_t key, pval_t val);
extern int ll_lookup(struct linked_list* ll, pkey_t key);
extern int ll_remove(struct linked_list* ll, pkey_t key);
extern int ll_range(struct linked_list* ll, pkey_t low, pkey_t high, entry_t* res_arr);
extern void ll_print(struct linked_list* ll);
extern void ll_destory(struct linked_list* ll);

/*ll_gc.c*/
#define HP_NUM  2

#define MAX_THREAD_NUM 4

typedef size_t hp_t;

struct hp_node {
    hp_t addr;
    struct hp_node* next;
};

struct hp_item {
	hp_t hp_arr[HP_NUM];
	int free_cnt;
	struct hp_node* free_list;
};

// extern void gc_init(int *tid);
// extern void gc_thread_init(int *tid);
// extern void gc_thread_release();

// extern struct hp_item* hp_item_setup(struct linked_list* ll, int tid);
// extern void hp_setdown(struct linked_list* ll);
// extern void hp_save_addr(struct hp_item* hp, int level, int index, hp_t hp_addr);
// extern void hp_clear_addr(struct hp_item* hp, int level, int index);
// extern hp_t hp_get_addr(struct hp_item* hp, int level, int index);
// extern void hp_clear_all_addr(struct hp_item* hp);
// extern void hp_retire_node(struct linked_list* ll, struct hp_item* hp, hp_t hp_addr);

#endif