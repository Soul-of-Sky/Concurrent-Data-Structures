#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "linked_list.h"
#include "atomic.h"

/*
 * hp_item_setup - hp initialization function
 */
struct hp_item* hp_item_setup(struct linked_list* ll, int tid) {
	struct hp_item* item;
    int old;

    while (1) {
   		old = ll->len; 
        if (cmpxchg2(&ll->len, old, old + HP_NUM)) {
            break;
        }
    }

	item = (struct hp_item*) malloc(sizeof(struct hp_item));
	item->free_cnt = 0;
	item->free_list = (struct hp_node*) malloc(sizeof(struct hp_node));
    item->free_list->next = NULL;
	
    ll->hp[tid] = item;

	return item;
}

/* 
 * hp_save_addr - save hp
 */
void hp_save_addr(struct hp_item* hp, int level, int index, hp_t hp_addr) {
	hp->hp_arr[index] = hp_addr;
}

/*
 * hp_clear_addr - release hp
 */
void hp_clear_addr(struct hp_item* hp, int level, int index) {
    hp->hp_arr[index] = 0;
}

hp_t hp_get_addr(struct hp_item* hp, int level, int index) {
    return hp->hp_arr[index];
}

void hp_clear_all_addr(struct hp_item* hp) {
    memset(hp->hp_arr, 0, sizeof(hp_t) * HP_NUM);
}

static void free_node(struct ll_node* ll_node) {
    free(ll_node);
}

/*
 * hp_retire_hp_item - when a thread exits, it ought to call hp_retire_hp_item() to retire the hp_item.
 */
static void hp_retire_hp_item(struct linked_list* ll, int tid) {
    struct hp_item* hp = ll->hp[tid];
	struct hp_node *curr, *prev;
    struct top_node_s *item;
	
    if (hp == NULL) {
		return;
    }

	ll->hp[tid] = NULL;
    curr = hp->free_list->next;
    while(curr) {
		item = curr->addr;
        free_node(item);

        prev = curr;
        curr = curr->next;

        free(prev);
		hp->free_cnt--;
    }
    free(hp->free_list);
    free(hp);
}

/*
 * hp_setdown - the master thread call this function at last only once!
 */
void hp_setdown(struct linked_list* ll) {
	int i;
    /* walk through the hp_list and free all the hp_items. */
    for (i = 0; i < MAX_THREAD_NUM; i++) {
        hp_retire_hp_item(ll, i);
    }
}

/*
 * hp_scan - scan and release node
 */
static void hp_scan(struct linked_list* ll, struct hp_item* hp) {
    int size = HP_NUM * MAX_THREAD_NUM;
    hp_t* hp_arr = (hp_t*) malloc(size * sizeof(hp_t));
    int len = 0, new_free_cnt = 0;
    struct hp_item* item;
	struct hp_node *new_free_list, *node;
	hp_t hp0;
	int i, j;

    for (i = 0; i < MAX_THREAD_NUM; i++) {
		item = ll->hp[i];
       	if (item == NULL) 
	    	continue;
        for(j = 0; j < HP_NUM; j++) {
            hp0 = ACCESS_ONCE(item->hp_arr[j]);
            if (hp0) {
                hp_arr[len++] = hp0;
            }
        }
    }

    //new head of the hp->free_list.
    new_free_list = (struct hp_node*) malloc(sizeof(struct hp_node));
    new_free_list->next = NULL;
	
    //walk through the hp->free_list.
    node = hp->free_list->next;  //the first node in the hp->free_list.
    while (node) {
        //pop the node from the hp->free_list.
        hp->free_list->next = node->next;
            
        //search the node->address in the hp_arr.
        hp0 = node->addr;
        for(i = 0; i < len; i++) {
            if (hp_arr[i] == hp0) {
                //found! push node into new_free_list.
                node->next = new_free_list->next;
                new_free_list->next = node;
                new_free_cnt++;
                break;
            }
        }
        if (i == len) {
			struct ll_node* ll_node = (struct ll_node*) hp0;
            free_node(ll_node);
            //the node can be freed too.
            free(node);
		}
        node = hp->free_list->next;
    }
    free(hp_arr);
    free(hp->free_list);
    hp->free_list = new_free_list;
    hp->free_cnt = new_free_cnt;
}

void hp_retire_node(struct linked_list* ll, struct hp_item* hp, hp_t hp_addr) {
    struct hp_node* node = (struct hp_node*) malloc(sizeof(struct hp_node));
    node->addr = hp_addr;

    //push the node into the hp->free_list. there's no contention between threads.
    node->next = hp->free_list->next;
    hp->free_list->next = node;

    hp->free_cnt++;
	if (hp->free_cnt > ll->len * HP_NUM) {
		hp_scan(ll, hp);
	}
}