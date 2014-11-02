#ifndef __BROKER_H
#define __BROKER_H

#include <event2/event.h>

#include "net.h"
#include "list.h"
#include "ght_hash_table.h"
#include "subcli.h"

typedef struct broker {
    char *cfg_path;
    char *pid_file;
    char *log_level;
    char *log_file;

    char *pub_ip;
    char *sub_ip;
    int pub_port;
    int sub_port;
    int pub_srv_fd;
    int sub_srv_fd;
    struct event_base *evloop;
    struct event *pub_ev;
    struct event *sub_ev;

    /* mapping from subscibe-client id to the subscribe-client */
    hashtable *subcli_table;
    /* mapping from subscibe key to a list of clients who subscribed this key */
    hashtable *subscibe_table;
    /* mapping from command name key to sub_command struct */
    hashtable *sub_commands;

    int pub_backlog;
    int sub_backlog;

    /* Error buffer for net.c */
    char neterr[NET_ERR_LEN];

    /* auto increasing counter for sub clients */
    int sub_inc_counter;

} broker;

typedef void subCommandProc(sub_client *c);

typedef struct subCommand {
    char *name;
    subCommandProc *proc;
} subCommand;

typedef struct sharedStruct {
    sds crlf;
    sds ok;
    sds err;
    sds pong;
} sharedStruct;

extern sharedStruct shared;
extern broker server;

#endif
