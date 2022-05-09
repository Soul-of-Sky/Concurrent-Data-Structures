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

static struct page* alloc_page(struct bp* bp, int type) {
    int is_inter = (type == BP_INTER) ? 1 : 0;
    int size = sizeof(struct page) + (bp->degree + is_inter)* sizeof(bp_kv_t);
    struct page* page = (struct page*) malloc(size);

    page->type = type;
    page->length = 0;
    page->kv[0].k = 0;
    rwlock_init(&page->lock);

    page->locked = 0;
    page->fa = NULL;

    page->next = NULL;

    return page;
}

extern struct bp* bp_init(unsigned int degree) {
    struct bp* bp = (struct bp*) malloc(sizeof(struct bp));

    bp->degree = degree;
    bp->root = alloc_page(bp, BP_LEAF);
    bp->locked = 0;
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
    page->locked = 1;
}

static void unlock_page(struct bp* bp, struct page* page, int op) {
    if (likely(page)) {
        if (!page->locked) {
            return;
        }
        if (op == BP_READ) {
            read_unlock(&page->lock);
        } else {
            write_unlock(&page->lock);
        }
        page->locked = 0;
    } else {
        if (!bp->locked) {
            return;
        }
        if (op == BP_READ) {
            read_unlock(&bp->lock);
        } else {
            write_unlock(&bp->lock);
        }
        bp->locked = 0;
    }
}

static void unlock_recurse(struct bp* bp, struct page* page, int op) {
    unlock_page(bp, page, op);
    if (likely(page)) {
        unlock_recurse(bp, page->fa, op);
    }
}

static void crabbing_unlock(struct bp* bp, struct page* page, int op) {
    if (op == BP_READ) {
        unlock_page(bp ,page->fa, op);
    } else {
        unlock_recurse(bp, page->fa, op);
    }
}

static struct page* get_page(struct bp* bp, struct page* page, int op) {
    crabbing_lock(page, op);
    if (is_safe(bp, page, op)) {
        crabbing_unlock(bp, page, op);
    }

    return page;
}

/* lock & get the root of b+tree */
/* need to protect the root of b+tree in case that the root may change when split*/
static struct page* get_bp_root(struct bp* bp, int op) {
    struct page* root;

    if (op == BP_READ) {
        read_lock(&bp->lock);
    } else {
        write_lock(&bp->lock);
    }
    bp->locked = 1;
    root = bp->root;

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
        new_page->fa = fa_page;
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
    new_page->fa = fa_page;
}

static struct page* split(struct bp* bp, struct page* page) {
    struct page* new_page = alloc_page(bp, page->type);
    struct page *fa_page, *new_fa_page;

    int i;
    int first_idx = (page->type == BP_INTER) ? 1 : 0;

    /*copy the right half to the new page*/
    for (i = page->length / 2; i < page->length; i++) {
        memcpy(&new_page->kv[i - page->length / 2 + first_idx], &page->kv[i], sizeof(bp_kv_t));
    }
    new_page->length = page->length / 2;
    
    /*set next before change length, otherwise there will be omission when scan*/
    if (page->type == BP_LEAF) {
        page->next = new_page;
    }
    page->length -= page->length / 2;
    /*the first key of INTER node is always zero*/

    fa_page = page->fa;
    if (fa_page) {
        /* check if father page need to split*/
        if (fa_page->length == bp->degree + 1) {
            new_fa_page = split(bp, fa_page);
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
    assert(k_cmp(leaf->kv[idx].k, k) <= 0);

    if (k_cmp(leaf->kv[idx].k, k) == 0) {
        unlock_recurse(bp, leaf, BP_ADD);
        return -EEXIST;
    }

    org_leaf = leaf;
    if (leaf->length == bp->degree) {
        new_leaf = split(bp, leaf);
        /* choose the correct leaf*/
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

    unlock_recurse(bp, org_leaf, BP_ADD);

    return 0;
}

extern int bp_lookup(struct bp* bp, ukey_t k, uval_t* v) {
    struct page* leaf;
    int idx;

    idx = find(bp, k, &leaf, BP_READ);
    assert(k_cmp(leaf->kv[idx].k, k) <= 0);

    if (k_cmp(leaf->kv[idx].k, k) == 0) {
        *v = leaf->kv[idx].v;
        unlock_recurse(bp ,leaf, BP_READ);
        return 0;
    }

    unlock_recurse(bp ,leaf, BP_READ);

    return -ENOENT;
}

static struct page* merge(struct bp* bp, struct page* page) {
    struct page *fa_page, *sibling, *sb_child_page;
    int idx, first_idx, i, is_left;

    fa_page = page->fa;
    if (fa_page == NULL) {
        return;
    }

    first_idx = (page->type == BP_INTER) ? 1 : 0; 
    idx = binary_search_in_page(fa_page, page->kv[first_idx].k);
    
    /* be aware that when merge INTER page, don't copy k[0](always 0) */
    if (idx + 1 < fa_page->length) {
        /* right sibling */
        sibling = (struct page*) fa_page->kv[idx + 1].v;
        is_left = 0;
    } else if (idx > 0) {
        /* left sibling */
        sibling = (struct page*) fa_page->kv[idx - 1].v;
        is_left = 1;
    } else if (page->length == 0 && page != bp->root) {
        // for (i = fa_page->length; i > idx; i--) {
        //     memcpy(&fa_page->kv[i], &fa_page->kv[i - 1], sizeof(bp_kv_t));
        // }
        // free_page(page);
        // goto end;
        /* not here */
    } else {
        return page;
    }

    if (page->length + sibling->length - first_idx <= bp->degree) {
        /* merge */
        write_lock(&sibling->lock);
        if (is_left) {
            for (i = first_idx; i < page->length; i++) {
                memcpy(&sibling->kv[sibling->length + i - first_idx], &page->kv[i], sizeof(bp_kv_t));
            }
        } else {
            for (i = sibling->length - 1; i >= first_idx; i--) {
                memcpy(&sibling->kv[i + sibling->length - first_idx], &sibling->kv[i], sizeof(bp_kv_t));
            }
            for (i = first_idx; i < page->length; i++) {
                memcpy(&sibling->kv[i], &page->kv[i], sizeof(bp_kv_t));
            }
        }
        if (page->type == BP_INTER) {
            for (i = first_idx; i < page->length; i++) {
                ((struct page*) page->kv[i].v)->fa = sibling;
            }
        } 
        sibling->length += page->length - first_idx;
        
        idx = binary_search_in_page(fa_page, page->kv[first_idx].k);

        for (i = fa_page->length; i > idx; i--) {
            memcpy(&fa_page->kv[i], &fa_page->kv[i - 1], sizeof(bp_kv_t));
        }
        
        free_page(page);
        page = sibling;
    } else {
        /*borrow one*/
        write_lock(&sibling->lock);
        if (is_left) {
            for (i = page->length; i > first_idx; i--) {
                memcpy(&page->kv[i], &page->kv[i - 1], sizeof(bp_kv_t));
            }
            memcpy(&page->kv[first_idx], &sibling->kv[sibling->length - 1], sizeof(bp_kv_t));

            sb_child_page = ((struct page*) sibling->kv[sibling->length - 1].v);
            write_lock(&sb_child_page->lock);
            sb_child_page->fa = page;
            write_unlock(&sb_child_page->lock);
        } else {
            memcpy(&page->kv[page->length], &sibling->kv[first_idx], sizeof(bp_kv_t));

            sb_child_page = ((struct page*) sibling->kv[first_idx].v);
            write_lock(&sb_child_page->lock);
            sb_child_page->fa = page;
            write_unlock(&sb_child_page->lock);

            for (i = first_idx; i < sibling->length - 1; i++) {
                memcpy(&sibling->kv[i], &sibling->kv[i + 1], sizeof(bp_kv_t));
            }
        }
        page->length++;
        sibling->length--;
        write_unlock(&sibling->lock);
    } 

end:
    if (fa_page->length < (bp->degree + 1 - 1) / 2 + 1) {
        merge(bp, fa_page);
    }

    return page;
}

extern int bp_remove(struct bp* bp, ukey_t k) {
    struct page *leaf;
    int idx;
    int i;

    idx = find(bp, k, &leaf, BP_DEL);
    assert(k_cmp(leaf->kv[idx].k, k) <= 0);

    if (k_cmp(leaf->kv[idx].k, k) < 0) {
        unlock_recurse(bp, leaf, BP_DEL);
        return -ENOENT;
    }

    for (i = idx + 1; i < leaf->length ; i++) {
        memcpy(&leaf->kv[i - 1], &leaf->kv[i], sizeof(bp_kv_t));
    }
    leaf->length--;

    if (leaf->length < (bp->degree - 1) / 2 + 1) {
        if (leaf->length == 0 && leaf->fa) {
            unlock_recurse(bp, leaf->fa, BP_DEL);
            free_page(leaf);
            return 0;
        } else {
            leaf = merge(bp, leaf);
        }
    }

    unlock_recurse(bp, leaf, BP_DEL);

    return 0;
}

extern int bp_range(struct bp* bp, ukey_t k, unsigned int len, uval_t* v_arr) {
    //TODO: link the leaf node
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
            __prefix = malloc(p_len + 4);
            strcpy(__prefix, prefix);
            strcat(__prefix, is_first ? "│   " : "    ");
            print_page(__prefix, p_len + 4, page->kv[i].k, (struct page*) page->kv[i].v, i == 0);
        } 
    } else if (page->type == BP_LEAF) {
        for (i = 0; i < page->length; i++) {
            __prefix = malloc(p_len + 4);
            strcpy(__prefix, prefix);
            strcat(__prefix, is_first ? "│   " : "    ");
            print_page(__prefix, p_len + 4, page->kv[i].k, NULL, i == 0);
        } 
    }

    free(__prefix);
}

extern void bp_print(struct bp* bp) {
    print_page("", 1, 0, bp->root, 1);
}
