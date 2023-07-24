#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/param.h>

#include "RingBuffer.h"

struct ringbuf_t{
	uint8_t *buf;
	uint8_t *head, *tail;
	size_t size;
};

/*
 * Return a pointer to one-past-the-end of the ring buffer's
 * contiguous buffer. You shouldn't normally need to use this function
 * unless you're writing a new ringbuf_* function.
 */
static const uint8_t* ringbuf_end(const struct ringbuf_t *rb) {
	return rb->buf + ringbuf_buffer_size(rb);
}

/*
 * Given a ring buffer rb and a pointer to a location within its
 * contiguous buffer, return the a pointer to the next logical
 * location in the ring buffer.
 */
static uint8_t* ringbuf_nextp(struct ringbuf_t * rb, const uint8_t *p) {
	/*
	 * The assert guarantees the expression (++p - rb->buf) is
	 * non-negative; therefore, the modulus operation is safe and
	 * portable.
	 */
	return rb->buf + ((++p - rb->buf) % ringbuf_buffer_size(rb));
}

/*
 * Create a new ring buffer with the given capacity (usable
 * bytes). Note that the actual internal buffer size may be one or
 * more bytes larger than the usable capacity, for bookkeeping.
 *
 * Returns the new ring buffer object, or 0 if there's not enough
 * memory to fulfill the request for the given capacity.
 */
struct ringbuf_t* ringbuf_new(size_t capacity) {
	struct ringbuf_t * rb = malloc(sizeof(struct ringbuf_t));
	if (rb) {

		/* One byte is used for detecting the full condition. */
		rb->size = capacity + 1;
		rb->buf = malloc(rb->size);
		if (rb->buf)
			ringbuf_reset(rb);
		else {
			free(rb);
			return 0;
		}
	}
	return rb;
}

/*
 * The size of the internal buffer, in bytes. One or more bytes may be
 * unusable in order to distinguish the "buffer full" state from the
 * "buffer empty" state.
 *
 * For the usable capacity of the ring buffer, use the
 * ringbuf_capacity function.
 */
size_t ringbuf_buffer_size(const struct ringbuf_t *rb) {
	return rb->size;
}

/*
 * Deallocate a ring buffer, and, as a side effect, set the pointer to
 * 0.
 */
void ringbuf_free(struct ringbuf_t *rb)  {
	free((*rb).buf);
	free(rb);
	rb = 0;
}

/*
 * Reset a ring buffer to its initial state (empty).
 */
void ringbuf_reset(struct ringbuf_t * rb) {
	rb->head = rb->tail = rb->buf;
}
/*
 * The usable capacity of the ring buffer, in bytes. Note that this
 * value may be less than the ring buffer's internal buffer size, as
 * returned by ringbuf_buffer_size.
 */
size_t ringbuf_capacity(const struct ringbuf_t *rb)  {
	return ringbuf_buffer_size(rb) - 1;
}

/*
 * The number of free/available bytes in the ring buffer. This value
 * is never larger than the ring buffer's usable capacity.
 */
size_t ringbuf_bytes_free(const struct ringbuf_t *rb)  {
	if (rb->head >= rb->tail)
		return ringbuf_capacity(rb) - (rb->head - rb->tail);
	else
		return rb->tail - rb->head - 1;
}

/*
 * The number of bytes currently being used in the ring buffer. This
 * value is never larger than the ring buffer's usable capacity.
 */
size_t ringbuf_bytes_used(const struct ringbuf_t *rb) {
	return ringbuf_capacity(rb) - ringbuf_bytes_free(rb);
}

int ringbuf_is_full(const struct ringbuf_t *rb) {
	return ringbuf_bytes_free(rb) == 0;
}

int ringbuf_is_empty(const struct ringbuf_t *rb) {
	return ringbuf_bytes_free(rb) == ringbuf_capacity(rb);
}

/*
 * Beginning at ring buffer dst's head pointer, fill the ring buffer
 * with a repeating sequence of len bytes, each of value c (converted
 * to an unsigned char). len can be as large as you like, but the
 * function will never write more than ringbuf_buffer_size(dst) bytes
 * in a single invocation, since that size will cause all bytes in the
 * ring buffer to be written exactly once each.
 *
 * Note that if len is greater than the number of free bytes in dst,
 * the ring buffer will overflow. When an overflow occurs, the state
 * of the ring buffer is guaranteed to be consistent, including the
 * head and tail pointers; old data will simply be overwritten in FIFO
 * fashion, as needed. However, note that, if calling the function
 * results in an overflow, the value of the ring buffer's tail pointer
 * may be different than it was before the function was called.
 *
 * Returns the actual number of bytes written to dst: len, if
 * len < ringbuf_buffer_size(dst), else ringbuf_buffer_size(dst).
 */
size_t ringbuf_memset(struct ringbuf_t * dst, int c, size_t len) {
	const uint8_t *bufend = ringbuf_end(dst);
	size_t nwritten = 0;
	size_t count = MIN(len, ringbuf_buffer_size(dst));
	int overflow = count > ringbuf_bytes_free(dst);

	while (nwritten != count) {

		/* don't copy beyond the end of the buffer */
		assert(bufend > dst->head);
		size_t n = MIN(bufend - dst->head, count - nwritten);
		memset(dst->head, c, n);
		dst->head += n;
		nwritten += n;

		/* wrap? */
		if (dst->head == bufend)
			dst->head = dst->buf;
	}

	if (overflow) {
		dst->tail = ringbuf_nextp(dst, dst->head);
		assert(ringbuf_is_full(dst));
	}

	return nwritten;
}

/*
 * Copy n bytes from a contiguous memory area src into the ring buffer
 * dst. Returns the ring buffer's new head pointer.
 *
 * It is possible to copy more data from src than is available in the
 * buffer; i.e., it's possible to overflow the ring buffer using this
 * function. When an overflow occurs, the state of the ring buffer is
 * guaranteed to be consistent, including the head and tail pointers;
 * old data will simply be overwritten in FIFO fashion, as
 * needed. However, note that, if calling the function results in an
 * overflow, the value of the ring buffer's tail pointer may be
 * different than it was before the function was called.
 */
void* ringbuf_memcpy_into(struct ringbuf_t *dst, const void *src, size_t count)  {
	const uint8_t *u8src = src;
	const uint8_t *bufend = ringbuf_end(dst);
	int overflow = count > ringbuf_bytes_free(dst);
	size_t nread = 0;

	while (nread != count) {
		/* don't copy beyond the end of the buffer */
		assert(bufend > dst->head);
		size_t n = MIN(bufend - dst->head, count - nread);
		memcpy(dst->head, u8src + nread, n);
		dst->head += n;
		nread += n;

		/* wrap? */
		if (dst->head == bufend)
			dst->head = dst->buf;
	}

	if (overflow) {
		dst->tail = ringbuf_nextp(dst, dst->head);
		assert(ringbuf_is_full(dst));
	}

	return dst->head;
};

/*
 * This convenience function calls read(2) on the file descriptor fd,
 * using the ring buffer rb as the destination buffer for the read,
 * and returns the value returned by read(2). It will only call
 * read(2) once, and may return a short count.
 *
 * It is possible to read more data from the file descriptor than is
 * available in the buffer; i.e., it's possible to overflow the ring
 * buffer using this function. When an overflow occurs, the state of
 * the ring buffer is guaranteed to be consistent, including the head
 * and tail pointers: old data will simply be overwritten in FIFO
 * fashion, as needed. However, note that, if calling the function
 * results in an overflow, the value of the ring buffer's tail pointer
 * may be different than it was before the function was called.
 */
ssize_t ringbuf_read(int fd, struct ringbuf_t * rb, size_t count)  {
	const uint8_t *bufend = ringbuf_end(rb);
	size_t nfree = ringbuf_bytes_free(rb);

	/* don't write beyond the end of the buffer */
	assert(bufend > rb->head);
	count = MIN(bufend - rb->head, count);
	ssize_t n = read(fd, rb->head, count);
	if (n > 0) {
		assert(rb->head + n <= bufend);
		rb->head += n;

		/* wrap? */
		if (rb->head == bufend)
			rb->head = rb->buf;

		/* fix up the tail pointer if an overflow occurred */
		if (n > nfree) {
			rb->tail = ringbuf_nextp(rb, rb->head);
			assert(ringbuf_is_full(rb));
		}
	}

	return n;
}

/*
 * Copy n bytes from the ring buffer src, starting from its tail
 * pointer, into a contiguous memory area dst. Returns the value of
 * src's tail pointer after the copy is finished.
 *
 * Note that this copy is destructive with respect to the ring buffer:
 * the n bytes copied from the ring buffer are no longer available in
 * the ring buffer after the copy is complete, and the ring buffer
 * will have n more free bytes than it did before the function was
 * called.
 *
 * This function will *not* allow the ring buffer to underflow. If
 * count is greater than the number of bytes used in the ring buffer,
 * no bytes are copied, and the function will return 0.
 */
void* ringbuf_memcpy_from(void *dst, struct ringbuf_t *src, size_t count) {
	size_t bytes_used = ringbuf_bytes_used(src);
	if (count > bytes_used)
		return 0;

	uint8_t *u8dst = dst;
	const uint8_t *bufend = ringbuf_end(src);
	size_t nwritten = 0;
	while (nwritten != count) {
		assert(bufend > src->tail);
		size_t n = MIN(bufend - src->tail, count - nwritten);
		memcpy(u8dst + nwritten, src->tail, n);
		src->tail += n;
		nwritten += n;

		/* wrap ? */
		if (src->tail == bufend)
			src->tail = src->buf;
	}

	return src->tail;
}

/*
 * This convenience function calls write(2) on the file descriptor fd,
 * using the ring buffer rb as the source buffer for writing (starting
 * at the ring buffer's tail pointer), and returns the value returned
 * by write(2). It will only call write(2) once, and may return a
 * short count.
 *
 * Note that this copy is destructive with respect to the ring buffer:
 * any bytes written from the ring buffer to the file descriptor are
 * no longer available in the ring buffer after the copy is complete,
 * and the ring buffer will have N more free bytes than it did before
 * the function was called, where N is the value returned by the
 * function (unless N is < 0, in which case an error occurred and no
 * bytes were written).
 *
 * This function will *not* allow the ring buffer to underflow. If
 * count is greater than the number of bytes used in the ring buffer,
 * no bytes are written to the file descriptor, and the function will
 * return 0.
 */
ssize_t ringbuf_write(int fd, struct ringbuf_t * rb, size_t count) {
	size_t bytes_used = ringbuf_bytes_used(rb);
	if (count > bytes_used)
		return 0;

	const uint8_t *bufend = ringbuf_end(rb);
	assert(bufend > rb->head);
	count = MIN(bufend - rb->tail, count);
	ssize_t n = write(fd, rb->tail, count);
	if (n > 0) {
		assert(rb->tail + n <= bufend);
		rb->tail += n;

		/* wrap? */
		if (rb->tail == bufend)
			rb->tail = rb->buf;

		assert(n + ringbuf_bytes_used(rb) == bytes_used);
	}

	return n;
}

/*
 * Copy count bytes from ring buffer src, starting from its tail
 * pointer, into ring buffer dst. Returns dst's new head pointer after
 * the copy is finished.
 *
 * Note that this copy is destructive with respect to the ring buffer
 * src: any bytes copied from src into dst are no longer available in
 * src after the copy is complete, and src will have 'count' more free
 * bytes than it did before the function was called.
 *
 * It is possible to copy more data from src than is available in dst;
 * i.e., it's possible to overflow dst using this function. When an
 * overflow occurs, the state of dst is guaranteed to be consistent,
 * including the head and tail pointers; old data will simply be
 * overwritten in FIFO fashion, as needed. However, note that, if
 * calling the function results in an overflow, the value dst's tail
 * pointer may be different than it was before the function was
 * called.
 *
 * It is *not* possible to underflow src; if count is greater than the
 * number of bytes used in src, no bytes are copied, and the function
 * returns 0.
 */
void* ringbuf_copy(struct ringbuf_t *dst, struct ringbuf_t *src, size_t count) {
	size_t src_bytes_used = ringbuf_bytes_used(src);
	if (count > src_bytes_used)
		return 0;
	int overflow = count > ringbuf_bytes_free(dst);

	const uint8_t *src_bufend = ringbuf_end(src);
	const uint8_t *dst_bufend = ringbuf_end(dst);
	size_t ncopied = 0;
	while (ncopied != count) {
		assert(src_bufend > src->tail);
		size_t nsrc = MIN(src_bufend - src->tail, count - ncopied);
		assert(dst_bufend > dst->head);
		size_t n = MIN(dst_bufend - dst->head, nsrc);
		memcpy(dst->head, src->tail, n);
		src->tail += n;
		dst->head += n;
		ncopied += n;

		/* wrap ? */
		if (src->tail == src_bufend)
			src->tail = src->buf;
		if (dst->head == dst_bufend)
			dst->head = dst->buf;
	}

	assert(count + ringbuf_bytes_used(src) == src_bytes_used);

	if (overflow) {
		dst->tail = ringbuf_nextp(dst, dst->head);
		assert(ringbuf_is_full(dst));
	}

	return dst->head;
}
