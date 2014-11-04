#ifndef __SUBCLI_H
#define __SUBCLI_H

#include <event2/event_struct.h>

#include "sds.h"
#include "ght_hash_table.h"
#include "constant.h"

#define REQ_INLINE      1
#define REQ_MULTIBULK   2

typedef struct sub_client {
    /* socket fd*/
    int fd;

    /* a 12-byte value likes MongoDB ObjectId */
    char *id;
    INT64 expire_time;

    /* pointer to registered event of this client */
    struct event *ev;

    sds read_buf;

    int wbufpos;
    char wbuf[SUB_WRITE_BUF_LEN];

    int req_type;
    int multi_bulk_len;
    int bulk_len;

    /* client command argc and argv */
    int argc;
    sds *argv;
} sub_client;

typedef void sub_command_proc(sub_client *c);

typedef struct sub_command {
    char *name;
    sub_command_proc *proc;
    int arity;
} sub_command;

sub_client *sub_cli_create(int fd, int inc_counter);
hashtable *sub_commands_init();
void reset_client(sub_client *c);
void sub_cli_release(sub_client *c);
void process_sub_read_buf(sub_client *c);
int subcli_event_update(sub_client *c, short event);
void send_reply_to_subcli(sub_client *c);
void add_reply_bulk(sub_client *c, sds cnt);

#endif
