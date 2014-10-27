#ifndef __BROKER_H
#define __BROKER_H

#include <event2/event.h>

#include "net.h"

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

    int pub_backlog;
    int sub_backlog;
    char neterr[NET_ERR_LEN];  /* Error buffer for net.c */
} broker;

extern broker server;

#endif
