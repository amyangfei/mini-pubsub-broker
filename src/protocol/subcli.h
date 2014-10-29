#ifndef __SUBCLI_H
#define __SUBCLI_H

#include "sds.h"

typedef struct sub_client {
    int fd;
    void *ev;
    sds read_buf;
    sds write_buf;
} sub_client;

sub_client *sub_cli_create(int fd);
void sub_cli_release(sub_client *c);

#endif
