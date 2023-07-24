#ifndef INCLUDED_RINGBUFFER_H
#define INCLUDED_RINGBUFFER_H

// Just a lib from github

#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ringbuf_t* ringbuf_new(size_t capacity);
size_t ringbuf_buffer_size(const struct ringbuf_t *rb);
void ringbuf_free(struct ringbuf_t *rb);
void ringbuf_reset(struct ringbuf_t *rb);
size_t ringbuf_capacity(const struct ringbuf_t *rb);
size_t ringbuf_bytes_free(const struct ringbuf_t *rb);
size_t ringbuf_bytes_used(const struct ringbuf_t *rb);
int ringbuf_is_full(const struct ringbuf_t *rb);
int ringbuf_is_empty(const struct ringbuf_t *rb);
size_t ringbuf_memset(struct ringbuf_t *dst, int c, size_t len);
void* ringbuf_memcpy_into(struct ringbuf_t *dst, const void *src, size_t count);
ssize_t ringbuf_read(int fd, struct ringbuf_t *rb, size_t count);
void* ringbuf_memcpy_from(void *dst, struct ringbuf_t *src, size_t count);
ssize_t ringbuf_write(int fd, struct ringbuf_t *rb, size_t count);
void* ringbuf_copy(struct ringbuf_t *dst, struct ringbuf_t *src, size_t count);

#ifdef __cplusplus
}
#endif

#endif
