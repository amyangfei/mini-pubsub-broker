#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <event2/event.h>
#include <datrie/trie.h>

#include "util.h"
#include "subcli.h"
#include "zmalloc.h"
#include "broker.h"
#include "event.h"
#include "trie_util.h"
#include "hset.h"

static void set_protocol_err(sub_client *c, int pos);
static int process_multibulk_buffer(sub_client *c);
static int process_inline_buffer(sub_client *c);
static int process_command(sub_client *c);

static void ping_command(sub_client *c);
static void subscribe_command(sub_client *c);
static void unsubscribe_command(sub_client *c);

static int prepare_to_write(sub_client *c);
static void add_reply_bulklen(sub_client *c, sds cnt);
static void add_reply(sub_client *c, sds cnt);
static void add_reply_error_fmt(sub_client *c, const char *fmt, ...);
static void add_reply_error_length(sub_client *c, char *s, size_t len);
static void add_reply_string(sub_client *c, char *s, size_t len);

/* Subscribe client command table
 *
 * name: a string representing the command name.
 * proc: pointer to the C function implementing the command.
 * arity: number of arguments, it is possible to use -N to say >= N
 * */
struct sub_command sub_command_table[] = {
    {"ping", ping_command, 1},
    {"subscribe", subscribe_command, -2},
    {"unsubscribe", unsubscribe_command, -1},
};

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

hashtable *sub_commands_init()
{
    hashtable *commands;

    int num_cmds = sizeof(sub_command_table) / sizeof(sub_command);
    commands = ght_create(3 * num_cmds);
    int i;
    for (i = 0; i < num_cmds; i++) {
        sub_command *cmd = sub_command_table + i;
        ght_insert(commands, cmd, strlen(cmd->name), cmd->name);
    }

    return commands;
}

static void free_client_argv(sub_client *c)
{
    int i;
    for (i = 0; i < c->argc; i++) {
        srv_log(LOG_DEBUG, "freeing client argv: %s", c->argv[i]);
        /*free(c->argv[i]);*/
        sdsfree(c->argv[i]);
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
            return SUBCLI_ERR;
        }

        /* Buffer should also contain \n */
        if (newline-(c->read_buf) > ((signed)sdslen(c->read_buf)-2)) {
            return SUBCLI_ERR;
        }

        ok = string2ll(c->read_buf+1, newline-(c->read_buf+1), &ll);
        if (!ok || ll > SUB_READ_BUF_LEN) {
            srv_log(LOG_ERROR, "protocol error: invalid multibulk length");
            set_protocol_err(c, pos);
            return SUBCLI_ERR;
        }

        pos = (newline - c->read_buf) + 2;
        if (ll <= 0) {
            sdsrange(c->read_buf, pos, -1);
            return SUBCLI_OK;
        }

        c->multi_bulk_len = ll;
        c->argv = malloc(sizeof(sds) * c->multi_bulk_len);
    }

    while (c->multi_bulk_len) {
        /* Read bulk length if unknown */
        if (c->bulk_len == -1) {
            newline = strchr(c->read_buf + pos, '\r');
            if (newline == NULL) {
                if (sdslen(c->read_buf) > MAX_INLINE_READ) {
                    srv_log(LOG_ERROR, "protocol error: too big bulk count string");
                    set_protocol_err(c, 0);
                    return SUBCLI_ERR;
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
                return SUBCLI_ERR;
            }

            ok = string2ll(c->read_buf+pos+1, newline-(c->read_buf+pos+1), &ll);
            if (!ok || ll < 0 || ll > MAX_BULK_LEN) {
                srv_log(LOG_ERROR, "Protocol error: invalid bulk length");
                set_protocol_err(c, pos);
                return SUBCLI_ERR;
            }

            pos += newline - (c->read_buf + pos) + 2;
            c->bulk_len = ll;
        }

        /* Read bulk argument */
        if (sdslen(c->read_buf) - pos < (unsigned)(c->bulk_len + 2)) {
            /* Not enough data (+2 == trailing \r\n) */
            break;
        } else {
            /*c->argv[c->argc++] = create_string_obj(c->read_buf+pos, c->bulk_len);*/
            c->argv[c->argc++] = sdsnewlen(c->read_buf+pos, c->bulk_len);
            pos += c->bulk_len + 2;
        }
        c->bulk_len = -1;
        c->multi_bulk_len--;
    }

    /* Trim to pos */
    if (pos) sdsrange(c->read_buf, pos, -1);

    /* We're done when c->multibulk == 0 */
    if (c->multi_bulk_len == 0) return SUBCLI_OK;

    /* Still not read to process the command */
    return SUBCLI_ERR;
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
        return SUBCLI_ERR;
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
        return SUBCLI_ERR;
    }

    sdsrange(c->read_buf, querylen+2, -1);

    /*c->argv = (char **) malloc(sizeof(char *) * argc);*/
    c->argv = (char **) malloc(sizeof(sds) * argc);
    for (c->argc = 0, i = 0; i < argc; i++) {
        if (sdslen(argv[i])) {
            /*c->argv[c->argc] = strdup(argv[i]);*/
            c->argv[c->argc] = sdsnewlen(argv[i], strlen(argv[i]));
            c->argc++;
        } else {
            sdsfree(argv[i]);
        }
    }
    zfree(argv);
    return SUBCLI_OK;
}

static int process_command(sub_client *c)
{
    srv_log(LOG_DEBUG, "c->argv[0]: %s", c->argv[0]);
    sds name = sdsnew(c->argv[0]);
    sdstolower(name);

    sub_command *cmd = ght_get(server.sub_commands, sdslen(name), name);
    if (!cmd) {
        add_reply_error_fmt(c, "unknown command '%s'", name);
    } else if ((cmd->arity > 0 && cmd->arity != c->argc) ||
               (c->argc < -cmd->arity)) {
        add_reply_error_fmt(c, "wrong number of arguments for '%s' command",
                name);
    } else {
        srv_log(LOG_DEBUG, "cmd name: %s arity: %d", cmd->name, cmd->arity);
        cmd->proc(c);
    }

    sdsfree(name);

    return SUBCLI_OK;
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
            if (process_inline_buffer(c) != SUBCLI_OK) break;
        } else if (c->req_type == REQ_MULTIBULK) {
            if (process_multibulk_buffer(c) != SUBCLI_OK) break;
        } else {
            srv_log(LOG_ERROR, "error req_type %d", c->req_type);
            exit(EXIT_FAILURE);
        }
    }

    if (c->argc == 0) {
        free_client_argv(c);
    } else {
        if (process_command(c) == SUBCLI_OK) {
            reset_client(c);
        }
    }
}

static void ping_command(sub_client *c)
{
    add_reply(c, shared.pong);
}

static int subscribe_channel(sub_client *c, sds channel)
{
    TrieData data;
    size_t len;
    AlphaChar *chan_alpha;

    len = sdslen(channel);
    chan_alpha  = (AlphaChar *) malloc(sizeof(AlphaChar) * len + 1);

    conv_to_alpha(server.dflt_to_alpha_conv, channel, chan_alpha, len+1);
    /* channel not exists in trie, create */
    if (!trie_retrieve(server.sub_trie, chan_alpha, &data)) {
        if (!trie_store(server.sub_trie, chan_alpha, TRIE_DATA_DFLT)) {
            srv_log(LOG_ERROR, "Failed to insert key %s into sub trie", channel);
            return SUBCLI_ERR;
        }
        /* create hashtable mapping from subscribe-channel to client id set */
        hset *hs = hset_create(SUB_SET_LEN);
        hset_insert(hs, CLIENT_ID_LEN, c->id, c);
        if (ght_insert(server.subscibe_table, hs, len, channel) == -1) {
            hset_release(hs);
            srv_log(LOG_ERROR,
                    "Failed to insert key[%s]->set into subscribe hashtable",
                    channel);
            return SUBCLI_ERR;
        }
    }

    return SUBCLI_OK;
}

static void subscribe_command(sub_client *c)
{
    int i;
    for (i = 1; i < c->argc; i++) {
        subscribe_channel(c, c->argv[i]);
    }
    add_reply_string(c, "+subscribe", 10);
    add_reply_string(c, "\r\n", 2);
}

static void unsubscribe_command(sub_client *c)
{
    add_reply_string(c, "+unsubscribe", 12);
    add_reply_string(c, "\r\n", 2);
}

int subcli_event_update(sub_client *c, short event)
{
    event_del(c->ev);

    struct event *sub_ev_new = event_new(server.evloop, c->fd, event,
            sub_ev_handler, c);
    if (sub_ev_new == NULL) {
        free(c);
        close(c->fd);
        srv_log(LOG_ERROR, "update sub client failed");
        return SUBCLI_ERR;
    }
    if (event_add(sub_ev_new, NULL) == -1) {
        event_del(sub_ev_new);
        return SUBCLI_ERR;
    }
    c->ev = sub_ev_new;
    return SUBCLI_OK;
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

static int prepare_to_write(sub_client *c)
{
    short event = event_get_events(c->ev);
    if (!(event & EV_WRITE)) {
        return subcli_event_update(c, event | EV_WRITE);
    }
    return SUBCLI_OK;
}

void add_reply_bulk(sub_client *c, sds cnt)
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
        return SUBCLI_ERR;
    }

    memcpy(c->wbuf + c->wbufpos, s, len);
    c->wbufpos += len;
    return SUBCLI_OK;
}

static void add_reply(sub_client *c, sds cnt)
{
    if (prepare_to_write(c) == SUBCLI_ERR) {
        return;
    }

    if (add_reply_to_buf(c, cnt, sdslen(cnt)) != SUBCLI_OK) {
        srv_log(LOG_ERROR, "failed to add reply [%s] to sub client", cnt);
    }
}

static void add_reply_error_fmt(sub_client *c, const char *fmt, ...)
{
    size_t l, i;
    va_list ap;
    va_start(ap, fmt);
    sds s = sdscatvprintf(sdsempty(), fmt, ap);
    va_end(ap);

    /* Make sure there are no newlines in the string, otherwise invalid protocol
     * is emitted. */
    l = sdslen(s);
    for (i = 0; i < l; i++) {
        if (s[i] == '\r' || s[i] == '\n') {
            s[i] = ' ';
        }
    }
    add_reply_error_length(c, s, sdslen(s));
    sdsfree(s);
}

static void add_reply_error_length(sub_client *c, char *s, size_t len)
{
    add_reply_string(c, "-ERR ", 5);
    add_reply_string(c, s, len);
    add_reply_string(c, "\r\n", 2);
}

static void add_reply_string(sub_client *c, char *s, size_t len)
{
    if (prepare_to_write(c) == SUBCLI_ERR) {
        return;
    }
    add_reply_to_buf(c, s, len);
}

