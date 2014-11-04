#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include "net.h"
#include "event.h"
#include "util.h"
#include "broker.h"
#include "pubcli.h"
#include "subcli.h"
#include "constant.h"

static int accept_tcp_handler(evutil_socket_t fd, short event, void *args);
static void pub_ev_handler(evutil_socket_t fd, short event, void *args);

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

static void pub_ev_handler(evutil_socket_t fd, short event, void *args)
{
    if (!(event & EV_READ)) {
        srv_log(LOG_WARN, "publisher [fd %d] invalid event: %d", event);
        return;
    }

    pub_client *c = (pub_client *) args;

    c->read_buf = sdsMakeRoomFor(c->read_buf, PUB_READ_BUF_LEN);
    size_t cur_len = sdslen(c->read_buf);
    int nread = read(fd, c->read_buf + cur_len, PUB_READ_BUF_LEN);
    if (nread == -1) {
        if (errno == EAGAIN || errno == EINTR) {
            /* temporary unavailable */
        } else {
            srv_log(LOG_ERROR, "[fd %d] failed to read from publisher: %s",
                    fd, strerror(errno));
            pub_cli_release(c);
        }
    } else if (nread == 0) {
        srv_log(LOG_INFO, "[fd %d] publisher detached", fd);
        pub_cli_release(c);
    } else {
        /*
        chunk[n] = '\0';
        srv_log(LOG_INFO, "[fd %d] publisher send: %s", fd, chunk);
        write(fd, "hello", 5);
        */
        /* data process */
        sdsIncrLen(c->read_buf, nread);
        process_pub_read_buf(c);
        /*process_publish(c, chunk, nread);*/
    }
}

void sub_ev_handler(evutil_socket_t fd, short event, void *args)
{
    sub_client *c = (sub_client *) args;

    if (event & EV_READ) {
        c->read_buf = sdsMakeRoomFor(c->read_buf, SUB_READ_BUF_LEN);
        size_t cur_len = sdslen(c->read_buf);
        int n = read(fd, c->read_buf + cur_len, SUB_READ_BUF_LEN);
        if (n == -1) {
            if (errno == EAGAIN || errno == EINTR) {
                /* temporary unavailable */
                return;
            } else {
                srv_log(LOG_ERROR, "[fd %d] failed to read from sublisher: %s",
                        fd, strerror(errno));
                sub_cli_release(c);
                return;
            }
        } else if (n == 0) {
            srv_log(LOG_INFO, "[fd %d] sublisher detached", fd);
            sub_cli_release(c);
            return;
        } else {
            sdsIncrLen(c->read_buf, n);
        }
        process_sub_read_buf(c);
    } else if (event & EV_WRITE) {
        send_reply_to_subcli(c);
    }
}

void accept_pub_handler(evutil_socket_t fd, short event, void *args)
{
    int cfd = accept_tcp_handler(fd, event, args);
    if (cfd == -1) {
        return;
    }
    pub_client *c = pub_cli_create(cfd);
    net_tcp_set_nonblock(NULL, cfd);
    net_enable_tcp_no_delay(NULL, cfd);
    struct event *pub_ev = event_new(server.evloop, cfd, EV_READ|EV_PERSIST,
            pub_ev_handler, c);
    if (pub_ev == NULL) {
        free(c);
        close(cfd);
        return;
    }
    event_add(pub_ev, NULL);
    c->ev = pub_ev;
}

void accept_sub_handler(evutil_socket_t fd, short event, void *args)
{
    int cfd = accept_tcp_handler(fd, event, args);
    if (cfd == -1) {
        return;
    }
    sub_client *c = sub_cli_create(cfd, ++server.sub_inc_counter);
    net_tcp_set_nonblock(NULL, cfd);
    net_enable_tcp_no_delay(NULL, cfd);
    struct event *sub_ev = event_new(server.evloop, cfd,
            EV_READ|EV_PERSIST, sub_ev_handler, c);
    if (sub_ev == NULL) {
        free(c);
        close(cfd);
        return;
    }
    event_add(sub_ev, NULL);
    c->ev = sub_ev;
    ght_insert(server.subcli_table, c, CLIENT_ID_LEN, c->id);
}

