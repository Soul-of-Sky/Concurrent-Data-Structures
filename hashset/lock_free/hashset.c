#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <limits.h>
#include <errno.h>

#include "hashset.h"

/*the first bit of a key is invalid*/
#define MASK 0x7fffffffffffffff

static uint64_t reverse_by_bit(ukey_t k) {
    unsigned int len = sizeof(k) * 8;
    uint64_t res = 0;
    int i;

    for (i = 0; i < len; i++) {
        if (k & (1UL << i)) {
            res |= 1UL << (len - 1 - i);
        }
    }

    return res;
}

/*sentinel key's last bit is 0*/
static uint64_t set_sentinel_key(ukey_t k) {
    return reverse_by_bit(k & MASK);
}

/*normal key's last bit is 1*/
static uint64_t set_key(ukey_t k) {
    return reverse_by_bit((k & MASK) | (~MASK));
}

/*get the father bucket's id of bucket[b_id]*/
static int get_fa_index(struct hash_set* hs, int b_id) {
    /*bucket whose sentinel key is xxxx0 split into xxxx0 and xxxx1*/
    /*so before reversed, the father of bucket[(0/1)xxxx] is 0xxxx*/
    int b_id_fence = hs->num_b;
    while(b_id_fence > b_id) {
        b_id_fence = b_id_fence >> 1;
    }

    return b_id - b_id_fence;
}

static struct bucket_list* get_bucket_list(struct hash_set* hs, int b_id) {
    int c_id = b_id / BUCKET_PER_CLUSTER;
    int c_b_id = b_id % BUCKET_PER_CLUSTER;
    cluster_t* cluster;
    struct bucket_list** buckets;
    struct bucket_list *bucket, *fa_bucket;
    int fa_b_id;
    struct ll_node* sentinel_node;
    int ret;

    if (likely(b_id != 0)) {
        fa_b_id = get_fa_index(hs, b_id);
        fa_bucket = get_bucket_list(hs, fa_b_id);
        assert(fa_bucket);
    }

    /*init a new cluster*/
    if (hs->clusters[c_id] == NULL) {
        cluster = (cluster_t*) malloc(sizeof(cluster_t));
        memset(cluster, 0, sizeof(cluster_t));
        if (!cmpxchg2(&hs->clusters[c_id], NULL, cluster)) {
            free(cluster);
        }
    }

    cluster = hs->clusters[c_id];
    buckets = (struct bucket_list**) cluster;
    bucket = buckets[c_b_id];
    /*init a new bucket list*/
    if (bucket == NULL) {
        bucket = (struct bucket_list*) malloc(sizeof(struct bucket_list));
        bucket->bucket_head = ll_create();
        ll_insert(bucket->bucket_head, set_sentinel_key(b_id), NULL);
        cluster = hs->clusters[c_id];
        if (!cmpxchg2(&(buckets[c_b_id]), NULL, bucket)) {
            ll_destroy(bucket->bucket_head);
            free(bucket);
        } else if (b_id != 0){
            /*insert the sentinel key into the lock-free linked list*/
            sentinel_node = GET_NODE(bucket->bucket_head->head->next);
            assert(!IS_MARKED(sentinel_node->next));
            ret = ll_insert2(fa_bucket->bucket_head, sentinel_node);
            assert(ret == 0);
        }
    }

    return buckets[c_b_id];
}

static struct bucket_list* get_bucket_list2(struct hash_set* hs, int b_id) {
    int c_id = b_id / BUCKET_PER_CLUSTER;
    int c_b_id = b_id % BUCKET_PER_CLUSTER;
    cluster_t* cluster;
    struct bucket_list** buckets;
    struct bucket_list *bucket;

    cluster = hs->clusters[c_id];
    if (cluster == NULL) {
        return NULL;
    }
    buckets = (struct bucket_list**) cluster;
    bucket = buckets[c_b_id];
    /*init a new bucket list*/

    return bucket;
}

extern struct hash_set* hs_create() {
    struct hash_set* hs = (struct hash_set*) malloc(sizeof(struct hash_set));
    
    hs->num_e = 0;
    hs->num_b = MIN_NUM_BUCKETS;
    get_bucket_list(hs, 0);

    return hs;
}

static void free_ll_node(struct ll_node* node) {
    free(node);
}

extern void hs_destroy(struct hash_set* hs) {
    struct bucket_list* bucket0 = get_bucket_list2(hs, 0);
    struct bucket_list* bucket;
    int i;

    ll_destroy(bucket0->bucket_head);

    for (i = 1; i < hs->num_b; i++) {
        bucket = get_bucket_list2(hs, i);
        if (bucket != NULL) {
            free_ll_node(bucket->bucket_head->head);
            free_ll_node(bucket->bucket_head->tail);
        }
    }
}

extern int hs_insert(struct hash_set* hs, ukey_t k, uval_t v) {
    int b_id = k % hs->num_b;
    struct bucket_list* bucket = get_bucket_list(hs, b_id);
    int num_e, num_b;

    assert(bucket);

    if (ll_insert(bucket->bucket_head, set_key(k), v) == -EEXIST) {
        return -EEXIST;
    }

    num_e = xadd(&hs->num_e, 1);
    num_b = ACCESS_ONCE(hs->num_b);

    if (1.0f * num_e / num_b > MAX_LOAD_FACTOR) {
        /*need to split*/
        if (num_b * 2 <= CLUSTER_LEN * BUCKET_PER_CLUSTER) {
            if (cmpxchg2(&hs->num_b, num_b, num_b * 2)) {
                printf("resize succeed: %d -> %d\n", num_b, num_b * 2);
            }
        } else {
            printf("resize failed!\n");
        }
    }

    return 0;
}

extern int hs_lookup(struct hash_set* hs, ukey_t k, uval_t* v) {
    int b_id = k % hs->num_b;
    struct bucket_list* bucket = get_bucket_list(hs, b_id);
    
    assert(bucket);

    return ll_lookup(bucket->bucket_head, set_key(k), v);
}

extern int hs_remove(struct hash_set* hs, ukey_t k) {
    int b_id = k % hs->num_b;
    struct bucket_list* bucket = get_bucket_list(hs, b_id);

    assert(bucket);

    return ll_remove(bucket->bucket_head, set_key(k));
}

extern void hs_print(struct hash_set* hs) {
    struct bucket_list* bucket0 = get_bucket_list(hs, 0);
    
    ll_print(bucket0->bucket_head);
}
