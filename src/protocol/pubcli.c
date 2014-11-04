#include <string.h>
#include <stdlib.h>
#include <event2/event.h>

#include "pubcli.h"
#include "subcli.h"
#include "zmalloc.h"
#include "trie_util.h"
#include "broker.h"
#include "util.h"
#include "hset.h"

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

static void single_chan_publish(sds msg, char *chan, size_t chan_len)
{
    hset *sub_set;
    hset_iterator iter;
    void *sub_cli;
    const void *client_id;

    /* subscibe set stores mapping from client_id to sub_client */
    sub_set = ght_get(server.subscibe_table, chan_len, chan);
    if (!sub_set) {
        return;
    }
    for (sub_cli = hset_first(sub_set, &iter, &client_id);
         sub_cli;
         sub_cli = hset_next(sub_set, &iter, &client_id)) {
        add_reply_bulk(sub_cli, msg);
    }
}

void process_pub_read_buf(pub_client *c)
{
    size_t len = sdslen(c->read_buf);
    char *prefix = (char *) malloc(len + 1);
    memcpy(prefix, c->read_buf, len + 1);

    TrieState *s = trie_root(server.sub_trie);
    AlphaChar *alpha = (AlphaChar *) malloc(sizeof(AlphaChar) * (len+1));
    conv_to_alpha(server.dflt_to_alpha_conv, c->read_buf, alpha, len+1);
    int last = -1, cur;
    while ((cur = trie_walker(s, alpha, len, last + 1)) != -1) {
        prefix[last+1] = c->read_buf[last+1];
        prefix[cur+1] = '\0';
        srv_log(LOG_DEBUG, "FOUND subscribe key: %s", prefix);
        single_chan_publish(c->read_buf, prefix, cur+1);
        last = cur;
    }

    free(alpha);
    free(prefix);
    trie_state_free(s);
    sdsclear(c->read_buf);
}
