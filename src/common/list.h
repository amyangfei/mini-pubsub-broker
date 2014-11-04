#ifndef __LIST_H
#define __LIST_H

#include <stddef.h>

#define LIST_OK     0
#define LIST_ERR    -1

typedef struct lkdList lkdList;

typedef void (lkd_list_free_fn) (void *data);


lkdList *lkd_list_create();
void lkd_list_release(lkdList **self_p);
void *lkd_list_head(lkdList *self);
void *lkd_list_tail(lkdList *self);
int lkd_list_append(lkdList *self, void *item);
int lkd_list_push(lkdList *self, void *item);
void *lkd_list_pop(lkdList *self);
int lkd_list_remove(lkdList *self, void *item);
void *lkd_list_set_freefn(lkdList *self, void *item, lkd_list_free_fn *fn,
        int at_tail);
lkdList *lkd_list_dup (lkdList *self);
size_t lkd_list_size(lkdList *self);
void lkd_list_autofree(lkdList *self);
void lkd_list_test();

#endif
