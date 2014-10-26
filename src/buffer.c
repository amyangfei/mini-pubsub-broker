#include <stdlib.h>
#include <string.h>

#include "buffer.h"

/* -------------------------- private prototypes ---------------------------- */
static mpsBuffer *_mps_buffer_make_room(mpsBuffer *buffer, size_t len);

mpsBuffer *mps_buffer_empty()
{
    mpsBuffer *buffer = (mpsBuffer *) malloc(sizeof(*buffer));
    if (!buffer) {
        return NULL;
    }

    buffer->buf = NULL;
    buffer->len = 0;
    buffer->used = 0;

    return buffer;
}

mpsBuffer *mps_buffer_from_string(const char *src, size_t len)
{
    mpsBuffer *buffer = (mpsBuffer *) malloc(sizeof(*buffer));
    if (!buffer || !src) {
        return NULL;
    }

    buffer->buf = (char *) malloc(len + 1);
    if (!buffer->buf) {
        free(buffer);
        return NULL;
    }
    buffer->len = len;
    buffer->used = len;
    memcpy(buffer->buf, src, len);
    buffer->buf[len] = '\0';

    return buffer;
}

static mpsBuffer *_mps_buffer_make_room(mpsBuffer *buffer, size_t len)
{
    if (!buffer) {
        return NULL;
    }

    if (buffer->len - buffer->used > len) {
        return buffer;
    }

    int new_len = 2 * (len + buffer->used);
    char *new_buf = (char *) realloc(buffer->buf, new_len);
    if (!new_buf) {
        return NULL;
    }

    buffer->buf = new_buf;
    buffer->len = new_len;

    return buffer;
}

int mps_buffer_append(mpsBuffer *buffer, const char *src, size_t len)
{
    if (!buffer || !src) {
        return BUFFER_ERR;
    }

    mpsBuffer *new_buffer = _mps_buffer_make_room(buffer, len);
    if (!new_buffer) {
        return BUFFER_ERR;
    }
    memcpy(new_buffer->buf + new_buffer->used, src, len);
    new_buffer->used += len;
    new_buffer->buf[new_buffer->used] = '\0';

    return BUFFER_OK;
}

void mps_buffer_reset(mpsBuffer *buffer)
{
    if (!buffer) {
        return;
    }

    free(buffer->buf);
    buffer->buf = NULL;
    buffer->len = 0;
    buffer->used = 0;
}

void mps_buffer_release(mpsBuffer *buffer)
{
    if (!buffer) {
        return;
    }

    free(buffer->buf);
    free(buffer);
}

void mps_buffer_shift(mpsBuffer *buffer, int start, size_t len)
{
    if (!buffer || start <= 0) {
        return;
    }

    if (start + len > buffer->used) {
        len = buffer->used - start;
    }

    memmove(buffer->buf, buffer->buf + start, len);
    buffer->used = len;
    buffer->buf[len] = '\0';
}

