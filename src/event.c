#include <unistd.h>

#include "net.h"
#include "event.h"
#include "util.h"
#include "broker.h"

static int accept_tcp_handler(evutil_socket_t fd, short event, void *args)
{
    int cport, cfd;
    char cip[IP_STR_LEN];
    (void) event;
    (void) args;

    cfd = net_tcp_accept(server.neterr, fd, cip, sizeof(cip), &cport);
    if (cfd == -1) {
        srv_log(LOG_WARN, "Accepting client connection: %s", server.neterr);
        return -1;
    }
    srv_log(LOG_INFO, "Accepted %s:%d", cip, cport);
    return cfd;
}

static void pub_read_handler(evutil_socket_t fd, short event, void *args)
{

}

void accept_pub_handler(evutil_socket_t fd, short event, void *args)
{
    int cfd = accept_tcp_handler(fd, event, args);
    if (cfd == -1) {
        return;
    }
    net_tcp_set_nonblock(NULL, cfd);
    net_enable_tcp_no_delay(NULL, cfd);
    struct event *pub_ev = event_new(server.evloop, cfd, EV_READ,
            pub_read_handler, NULL);
    if (pub_ev == NULL) {
        close(cfd);
        return;
    }
    event_add(pub_ev, NULL);
}

