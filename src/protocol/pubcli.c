#include <stdlib.h>
#include <event2/event.h>

#include "pubcli.h"
#include "zmalloc.h"

pub_client *pub_cli_create(int fd)
{
    pub_client *c = (pub_client *) zmalloc(sizeof(pub_client));
    if (!c) {
        return NULL;
    }
    c->fd = fd;
    c->ev = NULL;
    c->read_buf = sdsempty();
    c->write_buf = sdsempty();
    return c;
}

void pub_cli_release(pub_client *c)
{
    if (!c) {
        return;
    }
    sdsfree(c->read_buf);
    sdsfree(c->write_buf);
    event_del((struct event *)(c->ev));
    zfree(c);
}

