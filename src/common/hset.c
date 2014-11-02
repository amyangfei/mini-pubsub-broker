#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hset.h"

hset *hset_create(unsigned int size)
{
    hset *hs = (hset *) malloc(sizeof(hset));
    if (!hs) {
        return NULL;
    }

    hs->fake_val = (char *) malloc(FAKE_LEN + 1);
    memcpy(hs->fake_val, FAKE_VAL, FAKE_LEN);
    hs->fake_val[FAKE_LEN] = '\0';

    hs->table = ght_create(size);

    return hs;
}

void hset_release(hset *p_hset)
{
    free(p_hset->fake_val);
    ght_finalize(p_hset->table);
    free(p_hset);
}

int hset_size(hset *p_hset)
{
    return ght_size(p_hset->table);
}

int hset_insert(hset *p_hset, unsigned int key_size, const void *key_data)
{
    return ght_insert(p_hset->table, p_hset->fake_val, key_size, key_data);
}

void *hset_remove(hset *p_hset, unsigned int key_size, const void *key_data)
{
    return ght_remove(p_hset->table, key_size, key_data);
}

int hset_has(hset *p_hset, unsigned int key_size, const void *key_data)
{
    void *val = ght_get(p_hset->table, key_size, key_data);
    return val ? 1 : 0;
}

void *hset_first(hset *p_hset, hset_iterator *iter, const void **pp_key)
{
    return ght_first(p_hset->table, iter, pp_key);
}

void *hset_next(hset *p_hset, hset_iterator *iter, const void **pp_key)
{
    return ght_next(p_hset->table, iter, pp_key);
}

#ifdef SET_TEST_MAIN
#include <assert.h>

typedef struct key_test {
    char key[16];
} key_test;

int main(int argc, char *argv[])
{
    (void) argc;
    (void) argv;

    printf("set_test starts...\n");

    void *val;
    const void *key;
    hset_iterator iter;
    key_test keys[] = {
        {"one"}, {"two"}, {"three"}, {"four"},
        {"five"}, {"six"}, {"seven"}, {"eight"}
    };
    key_test keys_not[] = {
        {"eleven"}, {"twelve"}, {"thirteen"}, {"fourteen"},
    };
    int i;
    int keys_len = sizeof(keys) / sizeof(key_test);

    hset *hs = hset_create(64);

    for (i = 0; i < keys_len; i++) {
        hset_insert(hs, strlen(keys[i].key), keys[i].key);
    }
    assert(hset_size(hs) == keys_len);

    for (val = hset_first(hs, &iter, &key);
         val;
         val = hset_next(hs, &iter, &key)) {
        assert(strlen(val) == FAKE_LEN);
        assert(strncmp(FAKE_VAL, val, FAKE_LEN) == 0);
    }

    for (i = 0; i < keys_len; i++) {
        assert(hset_has(hs, strlen(keys[i].key), keys[i].key) == 1);
    }
    for (i = 0; i < keys_len; i++) {
        assert(hset_has(hs, strlen(keys_not[i].key), keys_not[i].key) == 0);
    }

    for (i = 0; i < keys_len; i++) {
        val = hset_remove(hs, strlen(keys[i].key), keys[i].key);
        assert(strlen(val) == FAKE_LEN);
        assert(strncmp(FAKE_VAL, val, FAKE_LEN) == 0);
    }

    hset_release(hs);

    printf("set_test ok\n");

    return 0;
}
#endif
