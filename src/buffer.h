#ifndef __BUFFER_H
#define __BUFFER_H

#define BUFFER_OK       0
#define BUFFER_ERR      -1

#include <unistd.h>

typedef struct mpsBuffer {
    char *buf;
    size_t len;
    size_t used;
} mpsBuffer;

mpsBuffer *mps_buffer_empty();
mpsBuffer *mps_buffer_from_string(const char *src, size_t len);
int mps_buffer_append(mpsBuffer *buffer, const char *src, size_t len);
void mps_buffer_reset(mpsBuffer *buffer);
void mps_buffer_release(mpsBuffer *buffer);
void mps_buffer_shift(mpsBuffer *buffer, int start, size_t len);

#endif
