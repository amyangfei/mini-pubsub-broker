#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <event2/event.h>

#include "util.h"
#include "subcli.h"
#include "zmalloc.h"
#include "broker.h"
#include "event.h"

static void set_protocol_err(sub_client *c, int pos);
static int process_multibulk_buffer(sub_client *c);
static int process_inline_buffer(sub_client *c);
static int process_command(sub_client *c);

static void ping_command(sub_client *c);

static void add_reply_bulklen(sub_client *c, sds cnt);
static void add_reply_bulk(sub_client *c, sds cnt);
static void add_reply(sub_client *c, sds cnt);

sub_client *sub_cli_create(int fd, int inc_counter)
{
    sub_client *c = (sub_client *) zmalloc(sizeof(sub_client));
    if (!c) {
        return NULL;
    }
    c->fd = fd;
    c->id = (char *) malloc(CLIENT_ID_LEN+1);
    c->id[CLIENT_ID_LEN] = '\0';
    create_objectid(c->id, inc_counter);
    c->ev = NULL;
    c->read_buf = sdsempty();
    c->wbufpos = 0;
    c->multi_bulk_len = 0;
    c->bulk_len = -1;
    return c;
}

static void free_client_argv(sub_client *c)
{
    int i;
    for (i = 0; i < c->argc; i++) {
        srv_log(LOG_DEBUG, "freeing client argv: %s", c->argv[i]);
        free(c->argv[i]);
    }
    if (c->argc > 0) {
        free(c->argv);
    }
    c->argc = 0;
}

void reset_client(sub_client *c)
{
    free_client_argv(c);
    c->req_type = 0;
    c->multi_bulk_len = 0;
    c->bulk_len = -1;
}

void sub_cli_release(sub_client *c)
{
    if (!c) {
        return;
    }
    sdsfree(c->read_buf);
    event_del((struct event *)(c->ev));
    zfree(c);
}

static void set_protocol_err(sub_client *c, int pos)
{
    srv_log(LOG_DEBUG, "Protocol error from sub client");
    sdsrange(c->read_buf, pos, -1);
}

static int process_multibulk_buffer(sub_client *c)
{
    char *newline = NULL;
    int pos = 0, ok;
    long long ll;

    if (c->multi_bulk_len == 0) {
        /* Multi bulk length cannot be read without a \r\n */
        newline = strchr(c->read_buf, '\r');
        if (newline == NULL) {
            if (sdslen(c->read_buf) > MAX_INLINE_READ) {
                srv_log(LOG_ERROR, "protocol error: too big mbulk string");
                set_protocol_err(c, 0);
            }
            return BROKER_ERR;
        }

        /* Buffer should also contain \n */
        if (newline-(c->read_buf) > ((signed)sdslen(c->read_buf)-2)) {
            return BROKER_ERR;
        }

        ok = string2ll(c->read_buf+1, newline-(c->read_buf+1), &ll);
        if (!ok || ll > SUB_READ_BUF_LEN) {
            srv_log(LOG_ERROR, "protocol error: invalid multibulk length");
            set_protocol_err(c, pos);
            return BROKER_ERR;
        }

        pos = (newline - c->read_buf) + 2;
        if (ll <= 0) {
            sdsrange(c->read_buf, pos, -1);
            return BROKER_OK;
        }

        c->multi_bulk_len = ll;
        c->argv = malloc(sizeof(char *) * c->multi_bulk_len);
    }

    while (c->multi_bulk_len) {
        /* Read bulk length if unknown */
        if (c->bulk_len == -1) {
            newline = strchr(c->read_buf + pos, '\r');
            if (newline == NULL) {
                if (sdslen(c->read_buf) > MAX_INLINE_READ) {
                    srv_log(LOG_ERROR, "protocol error: too big bulk count string");
                    set_protocol_err(c, 0);
                    return BROKER_ERR;
                }
                break;
            }

            /* Buffer should also contain \n */
            if (newline-(c->read_buf) > ((signed)sdslen(c->read_buf)-2)) {
                break;
            }

            if (c->read_buf[pos] != '$') {
                srv_log(LOG_ERROR, "Protocol error: expected '$', got '%c'",
                        c->read_buf[pos]);
                set_protocol_err(c, pos);
                return BROKER_ERR;
            }

            ok = string2ll(c->read_buf+pos+1, newline-(c->read_buf+pos+1), &ll);
            if (!ok || ll < 0 || ll > MAX_BULK_LEN) {
                srv_log(LOG_ERROR, "Protocol error: invalid bulk length");
                set_protocol_err(c, pos);
                return BROKER_ERR;
            }

            pos += newline - (c->read_buf + pos) + 2;
            c->bulk_len = ll;
        }

        /* Read bulk argument */
        if (sdslen(c->read_buf) - pos < (unsigned)(c->bulk_len + 2)) {
            /* Not enough data (+2 == trailing \r\n) */
            break;
        } else {
            c->argv[c->argc++] = create_string_obj(c->read_buf+pos, c->bulk_len);
            pos += c->bulk_len + 2;
        }
        c->bulk_len = -1;
        c->multi_bulk_len--;
    }

    /* Trim to pos */
    if (pos) sdsrange(c->read_buf, pos, -1);

    /* We're done when c->multibulk == 0 */
    if (c->multi_bulk_len == 0) return BROKER_OK;

    /* Still not read to process the command */
    return BROKER_ERR;
}

static int process_inline_buffer(sub_client *c)
{
    char *newline;
    int argc, i;
    sds *argv, aux;
    size_t querylen;

    newline = strchr(c->read_buf, '\n');

    if (newline == NULL) {
        if (sdslen(c->read_buf) > SUB_READ_BUF_LEN) {
            srv_log(LOG_ERROR, "Protocol error: too big inline request");
            set_protocol_err(c, 0);
        }
        return BROKER_ERR;
    }

    if (newline && newline != c->read_buf && *(newline-1) == '\r') {
        newline--;
    }

    querylen = newline - (c->read_buf);
    aux = sdsnewlen(c->read_buf, querylen);
    argv = sdssplitargs(aux, &argc);
    sdsfree(aux);
    if (argv == NULL) {
        set_protocol_err(c, 0);
        return BROKER_ERR;
    }

    sdsrange(c->read_buf, querylen+2, -1);

    c->argv = (char **) malloc(sizeof(char *) * argc);
    for (c->argc = 0, i = 0; i < argc; i++) {
        if (sdslen(argv[i])) {
            c->argv[c->argc] = strdup(argv[i]);
            c->argc++;
        } else {
            sdsfree(argv[i]);
        }
    }
    zfree(argv);
    return BROKER_OK;
}

static int process_command(sub_client *c)
{
    ping_command(c);

    return BROKER_OK;
}

void process_sub_read_buf(sub_client *c)
{
    while (sdslen(c->read_buf)) {
        if (c->read_buf[0] == '*') {
            c->req_type = REQ_MULTIBULK;
        } else {
            c->req_type = REQ_INLINE;
        }

        if (c->req_type == REQ_INLINE) {
            if (process_inline_buffer(c) != BROKER_OK) break;
        } else if (c->req_type == REQ_MULTIBULK) {
            if (process_multibulk_buffer(c) != BROKER_OK) break;
        } else {
            srv_log(LOG_ERROR, "error req_type %d", c->req_type);
            exit(EXIT_FAILURE);
        }
    }

    if (c->argc == 0) {
        free_client_argv(c);
    } else {
        if (process_command(c) == BROKER_OK) {
            reset_client(c);
        }
    }
    /*subcli_event_update(c, event_get_events(c->ev) | EV_WRITE);*/
    /*write(c->fd, "ok", 2);*/
}

static void ping_command(sub_client *c)
{
    add_reply(c, shared.pong);
}

void subcli_event_update(sub_client *c, short event)
{
    event_del(c->ev);

    struct event *sub_ev_new = event_new(server.evloop, c->fd, event,
            sub_ev_handler, c);
    if (sub_ev_new == NULL) {
        free(c);
        close(c->fd);
        srv_log(LOG_ERROR, "update sub client failed");
        return;
    }
    event_add(sub_ev_new, NULL);
    c->ev = sub_ev_new;
}

void send_reply_to_subcli(sub_client *c)
{
    int nwritelen, sentlen = 0;

    while (c->wbufpos > 0) {
        nwritelen = write(c->fd, c->wbuf+sentlen, c->wbufpos-sentlen);
        if (nwritelen <= 0) {
            break;
        }
        sentlen += nwritelen;
        if (sentlen == c->wbufpos) {
            c->wbufpos = 0;
        }
    }

    if (nwritelen == -1) {
        if (errno == EAGAIN) {
            return;
        } else {
            srv_log(LOG_ERROR, "Error writing to client: %s", strerror(errno));
            sub_cli_release(c);
            return;
        }
    }
    subcli_event_update(c, event_get_events(c->ev) & ~EV_WRITE);
}

static void add_reply_bulk(sub_client *c, sds cnt)
{
    add_reply_bulklen(c, cnt);
    add_reply(c, cnt);
    add_reply(c, shared.crlf);
}

static void add_reply_bulklen(sub_client *c, sds cnt)
{
    size_t len = sdslen(cnt);
    sds lensds = sdscatprintf(sdsempty(), "$%lu\r\n", len);
    add_reply(c, lensds);
}

static int add_reply_to_buf(sub_client *c, char *s, size_t len)
{
    size_t available = sizeof(c->wbuf) - c->wbufpos;
    if (len > available) {
        return BROKER_ERR;
    }

    memcpy(c->wbuf + c->wbufpos, s, len);
    c->wbufpos += len;
    return BROKER_OK;
}

static void add_reply(sub_client *c, sds cnt)
{
    short event = event_get_events(c->ev);
    if (!(event & EV_WRITE)) {
        subcli_event_update(c, event | EV_WRITE);
    }

    if (add_reply_to_buf(c, cnt, sdslen(cnt)) != BROKER_OK) {
        srv_log(LOG_ERROR, "failed to add reply [%s] to sub client", cnt);
    }
}
