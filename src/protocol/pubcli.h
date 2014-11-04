#ifndef __PUBCLI_H
#define __PUBCLI_H

#include "sds.h"

typedef struct pub_client {
    int fd;
    void *ev;
    sds read_buf;
    sds write_buf;
} pub_client;

pub_client *pub_cli_create(int fd);
void pub_cli_release(pub_client *c);
void process_pub_read_buf(pub_client *c);

#endif
