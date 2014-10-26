#ifndef __BROKER_H
#define __BROKER_H

#include <event.h>

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
    struct event_base *eventloop;
} broker;

extern broker server;

#endif
