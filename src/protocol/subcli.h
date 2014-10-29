#ifndef __SUBCLI_H
#define __SUBCLI_H

#include "sds.h"
#include "constant.h"

#define REQ_INLINE      1
#define REQ_MULTIBULK   2

typedef struct sub_client {
    int fd;

    INT64 expire_time;

    /* pointer to registered event of this client */
    void *ev;

    sds read_buf;
    sds write_buf;

    int req_type;
    int multi_bulk_len;
    int bulk_len;

    int argc;
    char **argv;
} sub_client;

sub_client *sub_cli_create(int fd);
void reset_client(sub_client *c);
void sub_cli_release(sub_client *c);
void process_sub_read_buf(sub_client *c);

#endif
