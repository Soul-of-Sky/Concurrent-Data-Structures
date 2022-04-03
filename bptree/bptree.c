#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <limits.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

#include "list.h"
#include "bptree.h"

#if 1
#define BP_ASSERT(x) assert(x)
#else
#define BP_ASSERT(x) do{}while(0)
#endif

static int page_size;

static struct page* alloc_page(enum page_type type) {
    int size = sizeof(struct page) + page_size * sizeof(bp_kv_t);
    struct page* page = (struct page*) malloc(size);

    page->type = type;
    page->length = 0;
    rwlock_init(&page->lock);
    
    return page;
}

extern struct bp* bp_init(unsigned int page_size) {
    struct bp* bp = (struct bp*) malloc(sizeof(struct bp));

    bp->head = alloc_page(LEAF);
    rwlock_init(&bp->lock);

    return bp;
}

static void free_page(struct page* page) {
    free(page);
}

static void free_all_pages(struct page* page) {
    int i;
    
    if (page->type == LEAF) {
        free_page(page);
        return;
    }

    for (i = 0; i < page->length; i++) {
        free_all_pages((struct page*) (page->kv[i].v));
    }
}

extern void bp_destroy(struct bp* bp) {
    free_all_pages(bp->head);
    free(bp);
}

static int binary_search_in_page(struct page* page, ukey_t k) {
    int l = 0, r = page->length;
    int mid, res = r;

    while(l < r) {
        mid = (l + r) >> 1;
        if (k_cmp(page->kv[mid].k, k) >= 0) {
            res = mid;
            r = mid;
        } else {
            l = mid + 1;
        }
    }
    
    return res;
}

extern int bp_insert(struct bp* bp, ukey_t k, uval_t v) {
    
}

extern int bp_lookup(struct bp* bp, ukey_t k, uval_t* v) {
    struct page *pred, *curr;
    int idx;

    spin_lock(&bp->lock);
    
    while(1) {
        idx = binary_search_in_page(curr, k);

        if (curr->type == LEAF) {
            //TODO: unlock

            if (idx == curr->length || k_cmp(curr->kv[idx].k, k) != 0) {
                *v = 0;
                return -EEXIST;
            } else {
                *v = curr->kv[idx].k;
                return 0;
            }
        }
        
        BP_ASSERT(idx < curr->length);
        
        
    }
}

extern int bp_remove(struct bp* bp, ukey_t k) {

}

extern int bp_range(struct bp* bp, ukey_t k, unsigned int len, uval_t* v_arr) {

}

extern void bp_print(struct bp* bp) {
    struct page *page;
    typedef struct {
        int level;
        ukey_t max;
        struct page* page;
        struct fifo fifo;
    }pg;
    pg *curr, *__pg;
    struct fifo* q = (struct fifo*) malloc(sizeof(struct fifo));
    int pred_level = 0, i;

    INIT_FIFO(q);
    __pg = (pg*) malloc(sizeof(pg)); 
    __pg->level = 0;
    __pg->max = 0;
    __pg->page = bp->head;
    fifo_in(__pg, q);

    while(!fifo_empty(q)) {
        curr = fifo_entry(fifo_out(q), pg, fifo);
        if (pred_level < curr->level) {
            pred_level++;
            printf("\n");
        }
        printf("[%d]: <", curr->max);
        page = curr->page;
        for (i = 0; i < page->length; i++) {
            __pg = (pg*) malloc(sizeof(pg)); 
            __pg->level = curr->level + 1;
            __pg->max = page->kv[i].k;
            __pg->page = (struct page*) page->kv[i].v;
            fifo_in(&__pg->fifo, q);
            if (i != 0) printf(", ");
            printf("%lu", page->kv[i].k);
        }
        printf("> ");
        
        free(curr);
    }
}
