#ifndef __CONSTANT_H
#define __CONSTANT_H

#include <stdint.h>

#define CONFIG_FILE_PATH    "./broker.conf"
#define PID_FILE_PATH       "./broker.pid"
#define LOG_FILE_PATH       "./broker.log"

#define PUB_IP_DLFT         "0.0.0.0"
#define SUB_IP_DLFT         "0.0.0.0"
#define PUB_PORT_DLFT       5561
#define SUB_PORT_DLFT       5562

#define TCP_PUB_BACKLOG     511
#define TCP_SUB_BACKLOG     511

#define PUB_READ_BUF_LEN    (1024*16)
#define SUB_READ_BUF_LEN    (1024*16)
#define SUB_WRITE_BUF_LEN   (1024*16)
#define MAX_INLINE_READ     (1024*16)
#define MAX_BULK_LEN        (1024*16)

#define CLIENT_ID_LEN       24

#define SIZE4               4
#define SIZE8               8
#define SIZE16              16
#define SIZE32              32
#define SIZE64              64
#define SIZE128             128
#define SIZE256             256
#define SIZE512             512
#define SIZE1024            1024

#define NET_ERR_LEN         SIZE256

#define SUB_SET_LEN         SIZE16

#define BROKER_OK           0
#define BROKER_ERR          -1

#define SUBCLI_OK           0
#define SUBCLI_ERR          -1

#define INT64               int64_t

#endif
