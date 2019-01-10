/*
 * Simple app that demonstrates how to setup an io_uring interface,
 * submit and complete IO against it, and then tear it down.
 *
 * gcc -Wall -O2 -D_GNU_SOURCE -o io_uring-test io_uring-test.c -luring
 */
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "../src/liburing.h"

#define QD	4

int main(int argc, char *argv[])
{
	struct io_uring ring;
	int i, fd, ret, pending, done;
	struct io_uring_sqe *sqe;
	struct io_uring_cqe *cqe;
	struct iovec *iovecs;
	off_t offset;
	void *buf;

	if (argc < 2) {
		printf("%s: file\n", argv[0]);
		return 1;
	}

	ret = io_uring_queue_init(QD, &ring, 0);
	if (ret < 0) {
		fprintf(stderr, "queue_init: %s\n", strerror(-ret));
		return 1;
	}

	fd = open(argv[1], O_RDONLY | O_DIRECT);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	iovecs = calloc(QD, sizeof(struct iovec));
	for (i = 0; i < QD; i++) {
		if (posix_memalign(&buf, 4096, 4096))
			return 1;
		iovecs[i].iov_base = buf;
		iovecs[i].iov_len = 4096;
	}

	offset = 0;
	i = 0;
	do {
		sqe = io_uring_get_sqe(&ring);
		if (!sqe)
			break;
		sqe->opcode = IORING_OP_READV;
		sqe->flags = 0;
		sqe->ioprio = 0;
		sqe->fd = fd;
		sqe->off = offset;
		sqe->addr = &iovecs[i];
		sqe->len = 1;
		sqe->buf_index = 0;
		offset += iovecs[i].iov_len;
	} while (1);

	ret = io_uring_submit(&ring);
	if (ret < 0) {
		fprintf(stderr, "io_uring_submit: %s\n", strerror(-ret));
		return 1;
	}

	done = 0;
	pending = ret;
	for (i = 0; i < pending; i++) {
		ret = io_uring_wait_completion(&ring, &cqe);
		if (ret < 0) {
			fprintf(stderr, "io_uring_get_completion: %s\n", strerror(-ret));
			return 1;
		}

		done++;
		if (cqe->res != 4096) {
			fprintf(stderr, "ret=%d, wanted 4096\n", cqe->res);
			return 1;
		}
	}

	printf("Submitted=%d, completed=%d\n", pending, done);
	close(fd);
	io_uring_queue_exit(&ring);
	return 0;
}
