#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <sched.h>
#include <time.h>
#include <errno.h>

#include "common.h"
#include "kv.h"

// #define RAND

#ifndef ENOENT
#define ENOENT  2
#endif

#ifndef EEXIST
#define EXIST   3
#endif

#ifndef N
#define N			10001
#endif

pkey_t a[5 * N];

#ifndef NUM_THREAD
#define NUM_THREAD	4
#endif

#define NUM_CPU		8

pthread_t tids[NUM_THREAD];

int sl_cmp(pkey_t a, pkey_t b) {
    if (a > b) return 1;
    if (a == b) return 0;
    return -1;
}

static int random_levels(struct sl_kv* sl) {
    int levels = 1;
    
    while (levels <= sl->levels && (rand() & 1)) {
        levels++;
    }
    
    if (levels > MAX_LEVELS)
        levels = MAX_LEVELS;

    if (levels > sl->levels) {
        SYNC_ADD(&sl->levels, 1);
    }
    
    return levels;
}

static struct kv_node* alloc_node(int levels) {
    struct kv_node* node;
    int size = sizeof(struct kv_node) + levels * sizeof(struct kv_node*);
    node = (struct kv_node*)malloc(size);
    assert(node);

    node->marked = 0;
    node->fully_linked = 0;

    spin_lock_init(&node->lock);
    memset(node->next, 0, sizeof(struct kv_node*) * levels);
    node->levels = levels;
    
    return node;
}

static void free_node(struct kv_node* node) {
    // free(node);
}

struct sl_kv* kv_init() {
    #ifndef UNRAND
    srand(time(0));
    #endif
    struct sl_kv* sl = (struct sl_kv*)malloc(sizeof(struct sl_kv));
    sl_head(sl) = alloc_node(MAX_LEVELS);
    sl_head(sl)->fully_linked = 1;
    sl_tail(sl) = alloc_node(MAX_LEVELS);
    sl_tail(sl)->fully_linked = 1;
    int i;
    for (i = 0; i < MAX_LEVELS; i++)
        sl_head(sl)->next[i] = sl_tail(sl);
    sl->levels = 1;

    return sl;
}

void kv_destory(struct sl_kv* sl) {
    struct kv_node* curr, *prev;

    curr = sl_head(sl);
    while(curr != sl_tail(sl)) {
        prev = curr;
        curr = curr->next[0];
        free(prev);
    }
    free(sl_tail(sl));
    free(sl);
}

// lowerbound in fact
void* kv_lookup(struct sl_kv* sl, pkey_t key) {
    struct kv_node *pred, *curr;
    int i;
    
    pred = sl_head(sl);
    for (i = sl_level(sl); i >= 0; i--) {
        curr = pred->next[i];
        while(curr != sl_tail(sl) && sl_cmp(key, node_key(curr)) > 0) {
            pred = curr;
            curr = pred->next[i];
        }
    }

    return node_val(curr);
}

static int kv_find(struct sl_kv* sl, pkey_t key, struct kv_node** preds, struct kv_node** succs) {
    struct kv_node *pred, *curr;
    int i, found_l = -1;

    pred = sl_head(sl);
    for (i = sl_level(sl); i >= 0; i--) {
        curr = pred->next[i];
        while(curr != sl_tail(sl) && sl_cmp(key, node_key(curr)) > 0) {
            pred = curr;
            curr = pred->next[i];
        }
        if (!found_l && sl_cmp(key, node_key(curr)) == 0) {
            found_l = i;
        }
        preds[i] = pred;
        succs[i] = curr;
    }
    
    return found_l;
}

int kv_insert(struct sl_kv* sl, pkey_t key, void* val) {
    int levels;
    struct kv_node* preds[MAX_LEVELS];
    struct kv_node* succs[MAX_LEVELS];
    struct kv_node *pred, *curr, *succ, *new_node;
    int found_l, i, high_l, full_locked;

    levels = random_levels(sl);
    while(1) {
        found_l = kv_find(sl, key, preds, succs);
        if (found_l != -1) {
            curr = succs[found_l];
            if (!curr->marked) {
                while(!curr->fully_linked);
                return -EEXIST;
            }
            continue;
        }

        full_locked = 1;
        for (i = 0; i < levels; i++) {
            pred = preds[i];
            succ = succs[i];
            if (i == 0 || pred != preds[i - 1]) {
                spin_lock(&pred->lock);
            }
            high_l = i;
            if (pred->marked || succ->marked || pred->next[i] != succ) {
                full_locked = 0;
                break;
            }
        }
        if (!full_locked) {
            for (i = 0; i <= high_l; i++) {
                pred = preds[i];
                if (i == 0 || pred != preds[i - 1]) {
                    spin_unlock(&pred->lock);
                }
            }
            continue;
        }

        new_node = alloc_node(levels);
        node_key(new_node) = key;
        node_val(new_node) = val;

        for (i = 0; i < levels; i++) {
            new_node->next[i] = succs[i];
            preds[i]->next[i] = new_node;
        }
        new_node->fully_linked = 1;

        for (i = 0; i < levels; i++) {
            pred = preds[i];
            if (i == 0 || pred != preds[i - 1]) {
                spin_unlock(&pred->lock);
            }
        }

        return 0;
    }
}

int kv_update(struct sl_kv* sl, pkey_t key, void* val) {
    int levels;
    struct kv_node* preds[MAX_LEVELS];
    struct kv_node* succs[MAX_LEVELS];
    struct kv_node *pred, *curr, *succ, *new_node;
    int found_l, i, high_l, full_locked;

    levels = random_levels(sl);
    while(1) {
        found_l = kv_find(sl, key, preds, succs);
        if (found_l != -1) {
            curr = succs[found_l];
            if (!curr->marked) {
                while(!curr->fully_linked);
                node_key(curr) = val;
                return 0;
            }
            continue;
        }

        full_locked = 1;
        for (i = 0; i < levels; i++) {
            pred = preds[i];
            succ = succs[i];
            if (i == 0 || pred != preds[i - 1]) {
                spin_lock(&pred->lock);
            }
            high_l = i;
            if (pred->marked || succ->marked || pred->next[i] != succ) {
                full_locked = 0;
                break;
            }
        }
        if (!full_locked) {
            for (i = 0; i <= high_l; i++) {
                pred = preds[i];
                if (i == 0 || pred != preds[i - 1]) {
                    spin_unlock(&pred->lock);
                }
            }
            continue;
        }

        new_node = alloc_node(levels);
        node_key(new_node) = key;
        node_val(new_node) = val;

        for (i = 0; i < levels; i++) {
            new_node->next[i] = succs[i];
            preds[i]->next[i] = new_node;
        }
        new_node->fully_linked = 1;

        for (i = 0; i < levels; i++) {
            pred = preds[i];
            if (i == 0 || pred != preds[i - 1]) {
                spin_unlock(&pred->lock);
            }
        }

        return 0;
    }
}

int kv_remove(struct sl_kv* sl, pkey_t key) {
    struct kv_node* preds[MAX_LEVELS];
    struct kv_node* succs[MAX_LEVELS];
    struct kv_node *victim, *pred;
    int found_l, i, is_marked, high_l, full_locked;

    while(1) {
        found_l = kv_find(sl, key, preds, succs);
        if (found_l != -1) {
            victim = succs[found_l];
        }
        if (is_marked || 
        (found_l != -1 && victim->fully_linked && victim->marked)) {
            if (!is_marked) {
                assert(found_l == victim->levels - 1);
                spin_lock(&victim->lock);
                if (victim->marked) {
                    spin_unlock(&victim->lock);
                    return -ENOENT;
                }
                victim->marked = 1;
                is_marked = 1;
            }

            full_locked = 1;
            for (i = 0; i <= found_l; i++) {
                pred = preds[i];
                if (i == 0 || pred != preds[i]) {
                    spin_lock(&pred->lock);
                }
                high_l = i;
                if (pred->marked || pred->next[i] != victim) {
                    full_locked = 0;
                    break;
                }
            }
            if (!full_locked) {
                for (i = 0; i <= high_l; i++) {
                    pred = preds[i];
                    if (i == 0 || pred != preds[i - 1]) {
                        spin_unlock(&pred->lock);
                    }
                }
                continue;
            }
            for (i = 0; i <= found_l; i++) {
                preds[i]->next[i] = victim->next[i];
            }
            spin_unlock(&victim->lock);
            free_node(victim);

            return 0;
        } else {
            return -ENOENT;
        }
    }
}

int kv_scan(struct sl_kv* sl, pkey_t min, pkey_t max) {
    return 0;
}

// single-thread ONLY
void kv_print(void* index_struct) {
	struct sl_kv *sl = (struct sl_kv*)index_struct;
    struct kv_node* node = sl_head(sl)->next[0];

	printf("index layer:\n");
    while(node != sl_tail(sl)) {
        printf("<%lu, %016lx> -> ", node->kv.k, node->kv.v);
        node = node->next[0];
    }

	printf("NULL\n");
}

void* thread_fun(void* arg) {
	struct sl_kv* sl;
    int i;

    sl = (struct sl_kv*) arg;
    for (i = 1; i <= 100000; i++) {
        kv_insert(sl, i, i);
    }
    printf("!!!\n");
    for (i = 1; i <= 100000; i++) {
        kv_remove(sl, i);
    }
}

void gen_data() {
	unsigned long i;

	for (i = 0; i < NUM_THREAD * N; i++) {
		a[i] = (pkey_t)i;
	}
#ifdef RAND
	srand(time(0));
	for (i = 1; i < NUM_THREAD * N; i++) {
        int swap_pos = rand() % (NUM_THREAD * N - i) + i;
        pkey_t temp = a[i];
        a[i] = a[swap_pos];
        a[swap_pos] = temp;
    }
#endif
}

int main() {
	long i;

	gen_data();
    
    struct sl_kv* sl = kv_init();

	for (i = 0; i < NUM_THREAD; i++) {
		pthread_create(&tids[i], NULL, thread_fun, (void*)sl);
	}

	for (i = 0; i < NUM_THREAD; i++) {
		pthread_join(tids[i], NULL);
	}

	return 0;
}
