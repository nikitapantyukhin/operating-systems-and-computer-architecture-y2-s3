#include "io_utils.h"

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

int write_all(int fd, const char *buffer, size_t size) {
    size_t total_written = 0;

    while (total_written < size) {
        ssize_t bytes_written = write(fd, buffer + total_written, size - total_written);

        if (bytes_written > 0) {
            total_written += (size_t)bytes_written;
            continue;
        }

        if (bytes_written == -1 && errno == EINTR) {
            continue;
        }

        if (bytes_written == -1) {
            perror("write");
            return -1;
        }

        fprintf(stderr, "write: wrote 0 bytes\n");
        return -1;
    }

    return 0;
}

int close_fd(int fd) {
    if (close(fd) == -1) {
        perror("close");
        return -1;
    }

    return 0;
}
