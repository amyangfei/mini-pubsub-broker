#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <event2/event.h>

#include "config.h"
#include "constant.h"
#include "net.h"
#include "util.h"
#include "event.h"
#include "broker.h"
#include "subcli.h"
#include "trie_util.h"

sharedStruct shared;

broker server;

static void create_shared_struct()
{
    shared.crlf = sdsnew("\r\n");
    shared.ok = sdsnew("+OK\r\n");
    shared.err = sdsnew("-ERR\r\n");
    shared.pong = sdsnew("+PONG\r\n");
}

void server_config_init()
{
    srand(time(NULL));

    server.cfg_path = strdup(CONFIG_FILE_PATH);
    server.pid_file = strdup(PID_FILE_PATH);
    server.log_level = strdup(LOG_DEBUG_STR);
    server.log_file = strdup(LOG_FILE_PATH);

    server.pub_ip = strdup(PUB_IP_DLFT);
    server.sub_ip = strdup(SUB_IP_DLFT);
    server.pub_port = PUB_PORT_DLFT;
    server.sub_port = SUB_PORT_DLFT;

    server.pub_srv_fd = -1;
    server.sub_srv_fd = -1;
    server.evloop = NULL;
    server.pub_ev = NULL;
    server.sub_ev = NULL;

    server.subcli_table = NULL;
    server.subscibe_table = NULL;
    server.sub_commands = NULL;

    server.pub_backlog = TCP_PUB_BACKLOG;
    server.sub_backlog = TCP_SUB_BACKLOG;

    server.sub_inc_counter = rand_int64(1 << 24);
}

void server_init()
{
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        return;
    }

    create_shared_struct();
    server.subcli_table = ght_create(SIZE512);
    server.subscibe_table = ght_create(SIZE512);
    server.sub_commands = sub_commands_init();
    server.sub_trie = trie_create();
    init_conv(&server.dflt_to_alpha_conv);

    server.evloop = event_base_new();
    if (!server.evloop) {
        srv_log(LOG_ERROR, "failed to initialize event loop");
        exit(EXIT_FAILURE);
    }

    int pub_fd = net_tcp_server(server.neterr, server.pub_port, server.pub_ip,
            server.pub_backlog);
    if (pub_fd == NET_ERR) {
        srv_log(LOG_ERROR, "opening socket: %s", server.neterr);
        exit(EXIT_FAILURE);
    }
    net_tcp_set_nonblock(NULL, pub_fd);
    server.pub_srv_fd = pub_fd;

    server.pub_ev = event_new(server.evloop, server.pub_srv_fd,
            EV_READ|EV_PERSIST, accept_pub_handler, NULL);
    event_add(server.pub_ev, NULL);

    int sub_fd = net_tcp_server(server.neterr, server.sub_port, server.sub_ip,
            server.sub_backlog);
    if (sub_fd == NET_ERR) {
        srv_log(LOG_ERROR, "opening socket: %s", server.neterr);
        exit(EXIT_FAILURE);
    }
    net_tcp_set_nonblock(NULL, sub_fd);
    server.sub_srv_fd = sub_fd;

    server.sub_ev = event_new(server.evloop, server.sub_srv_fd,
            EV_READ|EV_PERSIST, accept_sub_handler, NULL);
    event_add(server.sub_ev, NULL);

}

static void server_evloop_start()
{
    event_base_dispatch(server.evloop);
}

void server_free()
{
    srv_log(LOG_INFO, "freeing server");
    free(server.cfg_path);
    free(server.pid_file);
    free(server.log_level);
    free(server.log_file);
    free(server.pub_ip);
    free(server.sub_ip);
    if (server.pub_ev != NULL) event_free(server.pub_ev);
    if (server.sub_ev != NULL) event_free(server.sub_ev);
    if (server.evloop != NULL) event_base_free(server.evloop);
}

void usage()
{
    fprintf(stderr, "Usage: ./broker [OPTIONS]\n");
    fprintf(stderr, "  -c <config>\tPath to config file.\n");
    fprintf(stderr, "  -h         \tOutput this help and exit.\n");
}

int main(int argc, char *argv[])
{
    server_config_init();

    int opt;
    while ((opt = getopt(argc, argv,"c:h")) != -1) {
        switch (opt) {
            case 'c':
                free(server.cfg_path);
                server.cfg_path = strdup(optarg);
                break;
            case 'h':
                usage();
                exit(EXIT_SUCCESS);
            default:
                fprintf(stderr, "invalid option,type log_server -h for help\n");
                exit(EXIT_FAILURE);
        }
    }

    if (srv_load_cfg(server.cfg_path) == CONFIG_ERR) {
        fprintf(stderr, "error when loading config file\n");
        exit(EXIT_FAILURE);
    }

    /* log init */
    log_set_level(server.log_level);
    FILE *logger_fp = fopen(server.log_file, "a+");
    if (!logger_fp) {
        fprintf(stderr, "fail to open log file %s\n", server.log_file);
        exit(EXIT_FAILURE);
    }
    log_set_fp(logger_fp);

    /* pid file init */
    FILE *pid_fp = fopen(server.pid_file, "w");
    if (!pid_fp) {
        srv_log(LOG_ERROR, "failed to create pid file '%s': %s",
                server.pid_file, strerror(errno));
        exit(EXIT_FAILURE);
    }
    fprintf(pid_fp, "%ld\n", (long)getpid());
    fclose(pid_fp);

    server_init();

    srv_log(LOG_INFO, "starting main event loop");
    server_evloop_start();

    server_free();
    if (server.log_file) {
        fclose(logger_fp);
    }

    return 0;
}
