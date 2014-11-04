#ifndef __NET_H
#define __NET_H

#include <stddef.h>

#define NET_OK 0
#define NET_ERR -1

int net_tcp_server(char *err, int port, char *bindaddr, int backlog);
int net_tcp_set_nonblock(char *err, int fd);
int net_tcp_accept(char *err, int s, char *ip, size_t ip_len, int *port);
int net_enable_tcp_no_delay(char *err, int fd);
int net_disable_tcp_no_delay(char *err, int fd);

#endif
