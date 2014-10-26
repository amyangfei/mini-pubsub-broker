#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>

#include "config.h"
#include "constant.h"
#include "broker.h"
#include "util.h"

broker server;

void server_config_init()
{
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
    server.eventloop = NULL;
}

void server_init()
{
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
    event_base_free(server.eventloop);
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

    log_set_level(server.log_level);
    FILE *logger_fp = fopen(server.log_file, "a+");
    if (!logger_fp) {
        fprintf(stderr, "fail to open log file %s\n", server.log_file);
        exit(EXIT_FAILURE);
    }
    log_set_fp(logger_fp);

    FILE *pid_fp = fopen(server.pid_file, "w");
    if (!pid_fp) {
        srv_log(LOG_ERROR, "failed to create pid file '%s': %s",
                server.pid_file, strerror(errno));
        exit(EXIT_FAILURE);
    }
    fprintf(pid_fp, "%ld\n", (long)getpid());
    fclose(pid_fp);

    srv_log(LOG_INFO, "starting main event loop");
    /*server_event_loop_start();*/

    server_free();
    if (server.log_file) {
        fclose(logger_fp);
    }

    return 0;
}

