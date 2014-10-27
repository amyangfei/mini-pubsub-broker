#include <stdlib.h>
#include <event2/event.h>

#include "subcli.h"
#include "zmalloc.h"

sub_client *sub_cli_create(int fd)
{
    sub_client *c = (sub_client *) zmalloc(sizeof(sub_client));
    if (!c) {
        return NULL;
    }
    c->fd = fd;
    c->ev = NULL;
    c->read_buf = sdsempty();
    c->write_buf = sdsempty();
    return c;
}

void sub_cli_release(sub_client *c)
{
    if (!c) {
        return;
    }
    sdsfree(c->read_buf);
    sdsfree(c->write_buf);
    event_del((struct event *)(c->ev));
    zfree(c);
}
