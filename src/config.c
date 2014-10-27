#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sds.h"
#include "broker.h"
#include "util.h"
#include "cJSON.h"
#include "config.h"
#include "constant.h"

int srv_load_cfg(char *cfg_path)
{
    if (!cfg_path) {
        return CONFIG_OK;
    }

    FILE *fp = fopen(cfg_path, "r");
    if (!fp) {
        return CONFIG_OK;
    }

    char buf[SIZE1024];
    sds config_sds = sdsempty();
    while (fgets(buf, SIZE1024, fp) != NULL) {
        config_sds = sdscat(config_sds, buf);
    }

    cJSON *config_json = cJSON_Parse(config_sds);
    sdsfree(config_sds);
    if (!config_json) {
        srv_log(LOG_ERROR, "error while parsing config file: %s",
                cJSON_GetErrorPtr());
        return CONFIG_ERR;
    }

    /* update server config */
    cJSON *pub_ip = cJSON_GetObjectItem(config_json, "pub_ip");
    if (pub_ip) {
        free(server.pub_ip);
        server.pub_ip = strdup(pub_ip->valuestring);
    }

    cJSON *sub_ip = cJSON_GetObjectItem(config_json, "sub_ip");
    if (sub_ip) {
        free(server.sub_ip);
        server.sub_ip = strdup(sub_ip->valuestring);
    }

    cJSON *pub_port = cJSON_GetObjectItem(config_json, "pub_port");
    if (pub_port) {
        server.pub_port = pub_port->valueint;
    }

    cJSON *sub_port = cJSON_GetObjectItem(config_json, "sub_port");
    if (sub_port) {
        server.sub_port = sub_port->valueint;
    }

    cJSON *log_file = cJSON_GetObjectItem(config_json, "log_file");
    if (log_file) {
        server.log_file = strdup(log_file->valuestring);
    }

    cJSON *pid_file = cJSON_GetObjectItem(config_json, "pid_file");
    if (pid_file) {
        free(server.pid_file);
        server.pid_file = strdup(pid_file->valuestring);
    }

    cJSON_Delete(config_json);

    return CONFIG_OK;
}
