#include <stdlib.h>
#include <string.h>
#include <event2/event.h>

#include "util.h"
#include "subcli.h"
#include "zmalloc.h"

static void set_protocol_err(sub_client *c, int pos);
static int process_multibulk_buffer(sub_client *c);
static int process_inline_buffer(sub_client *c);
static int process_command(sub_client *c);

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
    sdsfree(c->write_buf);
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
    (void) c;
    srv_log(LOG_INFO, "inline command current not support: %s", c->read_buf);
    sdsclear(c->read_buf);
    return BROKER_OK;
}

static int process_command(sub_client *c)
{
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
}

