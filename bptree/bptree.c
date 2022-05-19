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

/* inter node */
/* <MIN_KEY, p0>, <k1, p1>, <k2, p2> ... <kn+1, pn+1>*/
/* max_key(p[i - 1]) < k[i]

/* leaf node */
/* <k0, v0>, <k1, v1>, <k2, v2> ... <kn, vn>*/

#define MAX_DEPTH 50
__thread rwlock_t* lock_trace[MAX_DEPTH];
__thread int lt_head, lt_tail;

static struct page* alloc_page(struct bp* bp, int type) {
    int is_inter = (type == BP_INTER) ? 1 : 0;
    int size = sizeof(struct page) + (bp->degree + is_inter)* sizeof(bp_kv_t);
    struct page* page = (struct page*) malloc(size);

    page->type = type;
    page->length = 0;
    page->kv[0].k = 0;
    rwlock_init(&page->lock);

    return page;
}

extern struct bp* bp_create(unsigned int degree) {
    struct bp* bp = (struct bp*) malloc(sizeof(struct bp));

    bp->degree = degree;
    bp->root = alloc_page(bp, BP_LEAF);
    rwlock_init(&bp->root_lock);
    rwlock_init(&bp->list_lock);
    INIT_LIST_HEAD(&bp->leaf_list);
    list_add(&bp->root->list, &bp->leaf_list);

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
    free_all_pages(bp->root);
    free(bp);
}

/* return the last index that k[idx] <= k*/
static int binary_search_in_page(struct page* page, ukey_t k) {
    int l = 0, r = page->length;
    int mid, res = r;

    /*lower bound*/
    while(l < r) {
        mid = (l + r) >> 1;
        if (k_cmp(page->kv[mid].k, k) >= 0) {
            res = mid;
            r = mid;
        } else {
            l = mid + 1;
        }
    }
    
    if (res == page->length) {
        return r - 1;
    }
    if (k_cmp(page->kv[res].k, k) == 0) {
        return res;
    } else {
        return res - 1;
    }
}

static int is_safe(struct bp* bp, struct page* page, int op) {
    int dp1 = page->type == BP_INTER ? 1 : 0;
    
    switch (op) {
    case BP_READ:
        return 1;
        break;
    case BP_ADD:
        return page->length < bp->degree + dp1;
        break;
    case BP_DEL:
        return page->length > (bp->degree + dp1 - 1) / 2 + 1;
        break;
    }
}

static void crabbing_lock(struct page* page, int op) {
    if (op == BP_READ) {
        read_lock(&page->lock);
    } else {
        write_lock(&page->lock);
    }
    lock_trace[lt_tail++] = &page->lock;
}

static void op_r_unlock() {
    read_unlock(lock_trace[lt_head++]);
    assert(lt_head == lt_tail - 1);
}

static void op_w_unlock() {
    for (; lt_head < lt_tail - 1; lt_head++) {
        write_unlock(lock_trace[lt_head]);
    }
}

static void crabbing_unlock(int op) {
    if (op == BP_READ) {
        op_r_unlock();
    } else {
        op_w_unlock();
    }
}

static void unlock_all(int op) {
    if (op == BP_READ) {
        for (; lt_head < lt_tail; lt_head++) {
            read_unlock(lock_trace[lt_head]);
        }
    } else {
        for (; lt_head < lt_tail; lt_head++) {
            write_unlock(lock_trace[lt_head]);
        }
    }
}

static struct page* get_page(struct bp* bp, struct page* page, int op) {
    crabbing_lock(page, op);
    if (is_safe(bp, page, op)) {
        crabbing_unlock(op);
    }

    return page;
}

/* lock & get the root of b+tree */
/* need to protect the root of b+tree in case that the root may change when split*/
static struct page* get_bp_root(struct bp* bp, int op) {
    struct page* root;

    lt_head = 0;
    lt_tail = 0;

    if (op == BP_READ) {
        read_lock(&bp->root_lock);
    } else {
        write_lock(&bp->root_lock);
    }
    lock_trace[lt_tail++] = &bp->root_lock;
    root = bp->root;
    get_page(bp, root, op);

    return root;
}

static int find(struct bp* bp, ukey_t k, struct page** leaf, int op) {
    struct page* page;
    int idx;

    page = get_bp_root(bp, op);
    while(1) {
        idx = binary_search_in_page(page, k);
        if (page->type == BP_LEAF) {
            *leaf = page;
            return idx;
        }
        page = (struct page*) page->kv[idx].v;
        get_page(bp, page, op);
    }
}

static void page_insert(struct page* fa_page, struct page* new_page) {
    int first_idx = (new_page->type == BP_INTER) ? 1 : 0;
    ukey_t k = new_page->kv[first_idx].k;
    int idx;
    int i;

    if (fa_page->length == 0) {
        fa_page->kv[0].v = (uval_t) new_page;
        fa_page->length++;
        return;
    }
    
    idx = binary_search_in_page(fa_page, k);
    assert(k_cmp(fa_page->kv[idx].k, k) < 0);

    for (i = fa_page->length - 1; i > idx; i--) {
        memcpy(&fa_page->kv[i + 1], &fa_page->kv[i], sizeof(bp_kv_t));
    }
    
    fa_page->kv[idx + 1].k = k;
    fa_page->kv[idx + 1].v = (uval_t) new_page;
    fa_page->length++;
}

static struct page* split(struct bp* bp, struct page* page, int lt_idx) {
    struct page* new_page = alloc_page(bp, page->type);
    struct page *fa_page, *new_fa_page;
    int length;
    int i;
    int first_idx = (page->type == BP_INTER) ? 1 : 0;

    length = (page->length - first_idx) / 2;
    /*copy the right half to the new page*/
    for (i = page->length - length; i < page->length; i++) {
        memcpy(&new_page->kv[i - (page->length - length) + first_idx], &page->kv[i], sizeof(bp_kv_t));
    }

    new_page->length = length + first_idx;
    page->length -= length;

    if (page->type == BP_LEAF) {
        write_lock(&bp->list_lock);
        list_add(&new_page->list, &page->list);
        write_unlock(&bp->list_lock);
    }

    if (page != bp->root) {
        /* check if father page need to split*/
        fa_page = container_of(lock_trace[lt_idx - 1], struct page, lock);
        if (fa_page->length == bp->degree + 1) {
            new_fa_page = split(bp, fa_page, lt_idx - 1);
            /* choose the correct father page*/
            if (k_cmp(new_fa_page->kv[1].k, new_page->kv[first_idx].k) <= 0) {
                fa_page = new_fa_page;
            }
        }
        page_insert(fa_page, new_page);
    } else {
        /*root is splited*/
        new_fa_page = alloc_page(bp, BP_INTER);
        page_insert(new_fa_page, page);
        page_insert(new_fa_page, new_page);
        bp->root = new_fa_page;
    }

    return new_page;
}

extern int bp_insert(struct bp* bp, ukey_t k, uval_t v) {
    struct page *leaf, *org_leaf, *new_leaf;
    int idx;
    int i;

    idx = find(bp, k, &leaf, BP_ADD);
    assert(idx == - 1 || k_cmp(leaf->kv[idx].k, k) <= 0);

    if (k_cmp(leaf->kv[idx].k, k) == 0) {
        unlock_all(BP_ADD);
        return -EEXIST;
    }

    org_leaf = leaf;
    if (leaf->length == bp->degree) {
        new_leaf = split(bp, leaf, lt_tail - 1);
        /* choose the correct leaf */
        if (k_cmp(new_leaf->kv[0].k, k) <= 0) {
            leaf = new_leaf;
            idx = binary_search_in_page(leaf, k);
        }
    }

    for (i = leaf->length - 1; i > idx; i--) {
        memcpy(&leaf->kv[i + 1], &leaf->kv[i], sizeof(bp_kv_t));
    }

    leaf->kv[idx + 1].k = k;
    leaf->kv[idx + 1].v = v;
    leaf->length++;

    unlock_all(BP_ADD);

    return 0;
}

extern int bp_lookup(struct bp* bp, ukey_t k, uval_t* v) {
    struct page* leaf;
    int idx;

    if (bp->root->length == 0) {
        return -ENOENT;
    }

    idx = find(bp, k, &leaf, BP_READ);
    assert(k_cmp(leaf->kv[idx].k, k) <= 0);

    if (k_cmp(leaf->kv[idx].k, k) == 0) {
        *v = leaf->kv[idx].v;
        unlock_all(BP_READ);
        return 0;
    }

    unlock_all(BP_READ);

    return -ENOENT;
}

static void merge(struct bp* bp, struct page* page, int lt_idx) {
    struct page *fa_page, *sibling;
    int idx, first_idx, i, is_left, length;

    if (page == bp->root) {
        if (page->length == 0) {
            page->type = BP_LEAF;
        }
        return;
    }

    fa_page = container_of(lock_trace[lt_idx - 1], struct page, lock);

    first_idx = (page->type == BP_INTER) ? 1 : 0; 
    idx = binary_search_in_page(fa_page, page->kv[first_idx].k);
    
    if (idx + 1 < fa_page->length) {
        /* always choose the right sibling, this is important because: */
        /* 1. prevent deadlock whiling ranging and removing at the same time */
        /* 2. k[0] is always the last key left in INTER node */
        sibling = (struct page*) fa_page->kv[idx + 1].v;
    } else if (page->length == 0) {
        for (i = idx; i < fa_page->length - 1; i--) {
            memcpy(&fa_page->kv[i], &fa_page->kv[i + 1], sizeof(bp_kv_t));
        }
        fa_page->length--;
        
        if (page != bp->root) {
            if (page->type == BP_LEAF) {
                write_lock(&bp->list_lock);
                list_del(&page->list);
                write_unlock(&bp->list_lock);
            }
            free_page(page);
            /* remember to change the lock_trace after free*/
            lt_tail--;
        }

        goto check_fa;
    } else {
        return;
    }

    if (page->length + sibling->length - first_idx <= bp->degree) {
        /* merge */
        /* be aware that when merge INTER page, don't copy k[0](always 0) */
        /* copy the right sibling and free right sibling*/
        write_lock(&sibling->lock);
        length = sibling->length - first_idx;
        for (i = first_idx; i < sibling->length; i++) {
            memcpy(&page->kv[page->length + i - first_idx], &sibling->kv[i], sizeof(bp_kv_t));
        }
        page->length += length;

        /* del from father page */
        idx = binary_search_in_page(fa_page, sibling->kv[first_idx].k);
        for (i = idx; i < fa_page->length - 1; i++) {
            memcpy(&fa_page->kv[i], &fa_page->kv[i + 1], sizeof(bp_kv_t));
        }
        fa_page->length--;

        /* delete in leaf list if it's leaf */
        if (sibling->type == BP_LEAF) {
            write_lock(&bp->list_lock);
            list_del(&sibling->list);
            write_unlock(&bp->list_lock);
        }

        /* free it */
        free_page(sibling);
    } else {
        /*borrow one*/
        write_lock(&sibling->lock);
        /* copy the min */
        memcpy(&page->kv[page->length], &sibling->kv[first_idx], sizeof(bp_kv_t));
        for (i = first_idx; i < sibling->length - 1; i++) {
            memcpy(&sibling->kv[i], &sibling->kv[i + 1], sizeof(bp_kv_t));
        }
        page->length++;
        sibling->length--;
        write_unlock(&sibling->lock);
        
        /*change the key in father page*/
        idx = binary_search_in_page(fa_page, sibling->kv[first_idx].k);
        fa_page->kv[idx].k = sibling->kv[first_idx].k;

        return;
    } 

check_fa:
    /* it must be a inter node */
    if (fa_page->length < (bp->degree + 1 - 1) / 2 + 1) {
        merge(bp, fa_page, lt_idx - 1);
    }

    return;
}

extern int bp_remove(struct bp* bp, ukey_t k) {
    struct page *leaf;
    int idx;
    int i;

    if (bp->root->length == 0) {
        return -ENOENT;
    }
    
    idx = find(bp, k, &leaf, BP_DEL);
    assert(k_cmp(leaf->kv[idx].k, k) <= 0);

    if (k_cmp(leaf->kv[idx].k, k) < 0) {
        unlock_all(BP_DEL);
        return -ENOENT;
    }

    for (i = idx + 1; i < leaf->length ; i++) {
        memcpy(&leaf->kv[i - 1], &leaf->kv[i], sizeof(bp_kv_t));
    }
    leaf->length--;

    if (leaf->length < (bp->degree - 1) / 2 + 1) {
        merge(bp, leaf, lt_tail - 1);
    }

    unlock_all(BP_DEL);

    return 0;
}

extern int bp_range(struct bp* bp, ukey_t k, unsigned int len, uval_t* v_arr) {
    struct list_head *list;
    struct page* page;
    int cnt = 0, found = 0;
    int i;

    read_lock(&bp->list_lock);

    list = &bp->leaf_list;
    list_for_each_entry(page, list, list) {
        i = 0;
        read_lock(&page->lock);
        if (found == 0) {
            if (k_cmp(page->kv[page->length - 1].k, k) >= 0) {
                for (; k_cmp(page->kv[i].k, k) < 0; i++);
                assert(i < page->length);
                found = 1;
            } else {
                read_unlock(&page->lock);
                continue;
            }
        }
        for (; i < page->length; i++) {
            v_arr[cnt++] = page->kv[i].v;
            if (cnt == len) {
                read_unlock(&page->lock);
                read_unlock(&bp->list_lock);
                return cnt;
            }
        }
        read_unlock(&page->lock);
    }

    read_unlock(&bp->list_lock);

    return cnt;
}

static void print_page(char* prefix, int p_len, ukey_t anchor, struct page* page, int is_first) {
    int i;
    char *__prefix = NULL;
    struct page* child_page;

    printf("%s", prefix);
    printf(is_first ? "├──" : "└──");
    printf("%lu\n", anchor);

    if (!page) {
        return;
    }
    if (page->type == BP_INTER) {
        for (i = 0; i < page->length; i++) {
            __prefix = malloc(p_len  + 10);
            strcpy(__prefix, prefix);
            strcat(__prefix, is_first ? "│   " : "    ");
            child_page = (struct page*) page->kv[i].v;
            print_page(__prefix, p_len  + 10, page->kv[i].k, child_page, i == 0);
        } 
    } else if (page->type == BP_LEAF) {
        for (i = 0; i < page->length; i++) {
            __prefix = malloc(p_len  + 10);
            strcpy(__prefix, prefix);
            strcat(__prefix, is_first ? "│   " : "    ");
            print_page(__prefix, p_len  + 10, page->kv[i].k, NULL, i == 0);
        } 
    }

    if (__prefix) free(__prefix);
}

extern void bp_print(struct bp* bp) {
    print_page("", 1, 0, bp->root, 1);
}
