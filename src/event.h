#ifndef __EVENT_H
#define __EVENT_H

#include <event2/util.h>

#define IP_STR_LEN INET6_ADDRSTRLEN

void accept_pub_handler(evutil_socket_t fd, short event, void *args);
void accept_sub_handler(evutil_socket_t fd, short event, void *args);

#endif
