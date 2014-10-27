#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <stdarg.h>

#include "net.h"

static void net_set_error(char *err, const char *fmt, ...)
{
    va_list ap;

    if (!err) return;
    va_start(ap, fmt);
    vsnprintf(err, NET_ERR_LEN, fmt, ap);
    va_end(ap);
}

static int net_listen(char *err, int s, struct sockaddr *sa, socklen_t len, int backlog) {
    if (bind(s,sa,len) == -1) {
        net_set_error(err, "bind: %s", strerror(errno));
        close(s);
        return NET_ERR;
    }

    if (listen(s, backlog) == -1) {
        net_set_error(err, "listen: %s", strerror(errno));
        close(s);
        return NET_ERR;
    }
    return NET_OK;
}

static int anetV6Only(char *err, int s) {
    int yes = 1;
    if (setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, &yes, sizeof(yes)) == -1) {
        net_set_error(err, "setsockopt: %s", strerror(errno));
        close(s);
        return NET_ERR;
    }
    return NET_OK;
}

static int net_set_reuse_addr(char *err, int fd) {
    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        net_set_error(err, "setsockopt SO_REUSEADDR: %s", strerror(errno));
        return NET_ERR;
    }
    return NET_OK;
}

static int _net_tcp_server(char *err, int port, char *bindaddr, int af, int backlog)
{
    int s, rv;
    char _port[6];  /* strlen("65535") */
    struct addrinfo hints, *servinfo, *p;

    snprintf(_port, 6, "%d", port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = af;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;    /* No effect if bindaddr != NULL */

    if ((rv = getaddrinfo(bindaddr,_port,&hints,&servinfo)) != 0) {
        net_set_error(err, "%s", gai_strerror(rv));
        return NET_ERR;
    }
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((s = socket(p->ai_family,p->ai_socktype,p->ai_protocol)) == -1)
            continue;

        if (af == AF_INET6 && anetV6Only(err, s) == NET_ERR) goto error;
        if (net_set_reuse_addr(err, s) == NET_ERR) goto error;
        if (net_listen(err, s, p->ai_addr, p->ai_addrlen, backlog) == NET_ERR) {
            goto error;
        }
        goto end;
    }
    if (p == NULL) {
        net_set_error(err, "unable to bind socket");
        goto error;
    }

error:
    s = NET_ERR;
end:
    freeaddrinfo(servinfo);
    return s;
}

int net_tcp_server(char *err, int port, char *bindaddr, int backlog)
{
    return _net_tcp_server(err, port, bindaddr, AF_INET, backlog);
}

int net_tcp_set_nonblock(char *err, int fd)
{
    int flags;

    if ((flags = fcntl(fd, F_GETFL)) == -1) {
        net_set_error(err, "fcntl(F_GETFL): %s", strerror(errno));
        return NET_ERR;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        net_set_error(err, "fcntl(F_SETFL,O_NONBLOCK): %s", strerror(errno));
        return NET_ERR;
    }
    return NET_OK;
}

static int net_generic_accept(char *err, int s, struct sockaddr *sa, socklen_t *len) {
    int fd;
    while(1) {
        fd = accept(s,sa,len);
        if (fd == -1) {
            if (errno == EINTR)
                continue;
            else {
                net_set_error(err, "accept: %s", strerror(errno));
                return NET_ERR;
            }
        }
        break;
    }
    return fd;
}

int net_tcp_accept(char *err, int s, char *ip, size_t ip_len, int *port) {
    int fd;
    struct sockaddr_storage sa;
    socklen_t salen = sizeof(sa);
    if ((fd = net_generic_accept(err, s,(struct sockaddr*)&sa, &salen)) == NET_ERR)
        return NET_ERR;

    if (sa.ss_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *)&sa;
        if (ip) inet_ntop(AF_INET,(void*)&(s->sin_addr),ip,ip_len);
        if (port) *port = ntohs(s->sin_port);
    } else {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&sa;
        if (ip) inet_ntop(AF_INET6,(void*)&(s->sin6_addr),ip,ip_len);
        if (port) *port = ntohs(s->sin6_port);
    }
    return fd;
}

static int net_set_tcp_no_delay(char *err, int fd, int val)
{
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val)) == -1) {
        net_set_error(err, "setsockopt TCP_NODELAY: %s", strerror(errno));
        return NET_ERR;
    }
    return NET_OK;
}

int net_enable_tcp_no_delay(char *err, int fd)
{
    return net_set_tcp_no_delay(err, fd, 1);
}

int net_disable_tcp_no_delay(char *err, int fd)
{
    return net_set_tcp_no_delay(err, fd, 0);
}
