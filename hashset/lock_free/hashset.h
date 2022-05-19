#ifndef HASHSET_H
#define HASHSET_H

#include "linked_list.h"
#include "ebr.h"

#define MIN_NUM_BUCKETS     2
#define MAX_LOAD_FACTOR     1
#define BUCKET_PER_CLUSTER  4096
#define CLUSTER_LEN         4096

struct bucket_list {
    struct ll* bucket_head;
};

typedef struct bucket_list* cluster_t[BUCKET_PER_CLUSTER];

struct hash_set {
    cluster_t* clusters[CLUSTER_LEN];
    float laod_factor;
    int num_b;
    int num_e;
    struct ebr* ebr;
};

extern struct hash_set* hs_create();
extern void hs_destroy(struct hash_set* hs);
extern int hs_insert(struct hash_set* hs, ukey_t k, uval_t v, int tid);
extern int hs_lookup(struct hash_set* hs, ukey_t k, uval_t* v, int tid);
extern int hs_remove(struct hash_set* hs, ukey_t k, int tid);
extern void hs_print(struct hash_set* hs);

#endif