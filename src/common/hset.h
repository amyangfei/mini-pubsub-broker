#ifndef __SET_H
#define __SET_H

#include "ght_hash_table.h"

#define FAKE_LEN    4
#define FAKE_VAL    "qnyh"

typedef struct hset {
    hashtable *table;
    char *fake_val;
} hset;

typedef ght_iterator_t hset_iterator;

hset *hset_create(unsigned int size);
int hset_size(hset *p_hset);
int hset_insert(hset *p_hset, unsigned int key_size, const void *key_data);
void *hset_remove(hset *p_hset, unsigned int key_size, const void *key_data);
int hset_has(hset *p_hset, unsigned int key_size, const void *key_data);
void *hset_first(hset *p_hset, hset_iterator *iter, const void **pp_key);
void *hset_next(hset *p_hset, hset_iterator *iter, const void **pp_key);

#endif
