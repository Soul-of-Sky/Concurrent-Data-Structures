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

#if 0
#define BP_ASSERT(x) assert(x)
#else
#define BP_ASSERT(x) do{}while(0)
#endif

static int page_size;

static struct page* alloc_page(int type, int is_head) {
    int size = sizeof(struct page) + page_size * sizeof(bp_kv_t);
    struct page* page = (struct page*) malloc(size);

    page->type = type;
    page->is_head = is_head;
    page->length = 0;
    rwlock_init(&page->lock);
    LIST_HEAD_INIT(page->list);

    return page;
}

extern struct bp* bp_init(unsigned int __page_size) {
    struct bp* bp = (struct bp*) malloc(sizeof(struct bp));

    page_size = __page_size;
    bp->head = alloc_page(LEAF);
    rwlock_init(&bp->lock);

    return bp;
}

static void free_page(struct page* page) {
    free(page);
}

static void free_all_pages(struct page* page) {
    int i;
    
    if (page->type == BP_LEAF) {
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
    
    if (res == r) {
        return r - 1;
    }
    if (k_cmp(page->kv[res].k, k) == 0) {
        return res;
    } else {
        return res - 1;
    }
}

typedef struct lock_trace {
    struct page* page;
    int is_page;
    struct list_head list;
}lock_trace_t;

static void lt_lock(struct list_head* list, struct page* page, int op, int is_page) {
    lock_trace_t* new_lt = (lock_trace_t*) malloc(sizeof(lock_trace_t));
    new_lt.page = page; 
    new_lt.is_page = is_page;
    
    if (likely(is_page)) {
        op == BP_READ ? read_lock(&page->lock) : write_lock(&page->lock);
    } else {
        op == BP_READ ? read_lock(&((struct bp*)page)->lock) : write_lock(&((struct bp*)page)->lock);
    }
    list_add(&new_lt, list);
}

static void lt_unlock(struct list_head* list, int op) {
    lock_trace_t *lt, *n;

    int first = 1;
    list_for_each_entry_safe(lt, n, list, list) {
        /* the first entry is the current page, skip it*/
        if (first) continue;
        first = 0;
        if (likely(lt->is_page)) {
            op == BP_READ ? read_unlock(&lt->page->lock) : write_unlock(&lt->page->lock);
        } else {
            op == BP_READ ? read_unlock(&((struct bp*)(lt->page))->lock) : write_unlock(&((struct bp*)(lt->page))->lock);
        }
        list_del(&lt->list);
        free(lt);
    }
}

static int is_safe(struct page* page, int op) {
    switch (op) {
    case BP_ADD:
        return page->length < page_size;
        break;
    case BP_DEL:
        return page->length > page_size / 2 + 1;
        break;
    }
}

static struct page* get_page(struct page* page, int op, struct list_head* list) {
    lt_lock(list, page, op, 1);
    if (is_safe(page)) {
        lt_unlock(list, op);
    }
    return page;
}

static struct page* get_bp_head(struct bp* bp, int op, struct list_head* list) {
    struct page* head;

    lt_lock(list, (struct page*)bp, op, 0);
    head = bp->head;
    get_page(head, op, list);

    return head;
}

static int find(struct bp* bp, ukey_t k, struct page** leaf) {
    struct page* page;
    int idx;

    page = get_bp_head(bp);
    while(1) {
        idx = binary_search_in_page(page, k);
        if (page->type == BP_LEAF) {
            *leaf = page;
            return idx;
        }
        page = (struct page*) page->kv[idx].v;
        get_page(page);
    }
}

extern int bp_insert(struct bp* bp, ukey_t k, uval_t v) {
    struct page* leaf;
    int idx;

    idx = find(bp, k, &leaf);
    
    
}

extern int bp_lookup(struct bp* bp, ukey_t k, uval_t* v) {
    struct page* leaf;
    int idx;

    idx = find(bp, k, &leaf);
}

extern int bp_remove(struct bp* bp, ukey_t k) {
    struct page* leaf;
    int idx;

    idx = find(bp, k, &leaf);
}

extern int bp_range(struct bp* bp, ukey_t k, unsigned int len, uval_t* v_arr) {

}

static void print_page(char* prefix, int p_len, ukey_t anchor, struct page* page, int is_first) {
    int i;
    char *__prefix;

    printf("%s", prefix);
    printf(is_first ? "├──" : "└──");
    printf("%lu\n", anchor);

    if (!page) {
        return;
    }
    if (page->type == BP_INTER) {
        for (i = 0; i < page->length; i++) {
            __prefix = realloc(prefix, p_len + 4);
            print_page(strcat(__prefix, is_first ? "│   " : "    "), p_len + 4, 
                    page->kv[i].k, (struct page*) page->kv[i].v, i == 0);
        } 
    } else if (page->type == BP_LEAF) {
        for (i = 0; i < page->length; i++) {
            __prefix = realloc(prefix, p_len + 4);
            print_page(strcat(__prefix, is_first ? "│   " : "    "), p_len + 4, 
                    page->kv[i].k, NULL, i == 0);
        } 
    } else {
        assert(0);
    }
}

extern void bp_print(struct bp* bp) {
    print_page("", 1, 0, bp->head, 1);
}
