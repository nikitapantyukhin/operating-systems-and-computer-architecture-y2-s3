#include "io_utils.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int cleanup_on_error(char *line, int file_fd) {
    free(line);
    close(file_fd);
    return 1;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Filename was not provided\n");
        return 1;
    }

    fprintf(stderr, "Child PID: %ld, Parent PID: %ld\n", (long)getpid(), (long)getppid());

    int file_fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (file_fd == -1) {
        perror("open");
        return 1;
    }

    char *line = NULL;
    size_t line_size = 0;

    while (getline(&line, &line_size, stdin) != -1) {
        long long sum = 0;
        char *token = strtok(line, " \t\r\n");

        if (token == NULL) {
            fprintf(stderr, "Empty input line\n");
            return cleanup_on_error(line, file_fd);
        }

        while (token != NULL) {
            errno = 0;
            char *end = NULL;
            long number = strtol(token, &end, 10);

            if (end == token || *end != '\0' || errno == ERANGE || number < INT_MIN || number > INT_MAX) {
                fprintf(stderr, "Invalid number: %s\n", token);
                return cleanup_on_error(line, file_fd);
            }

            sum += number;
            token = strtok(NULL, " \t\r\n");
        }

        char result[64];
        int result_length = snprintf(result, sizeof(result), "%lld\n", sum);

        if (result_length < 0) {
            fprintf(stderr, "Failed to format result\n");
            return cleanup_on_error(line, file_fd);
        }

        if (write_all(file_fd, result, (size_t)result_length) == -1) {
            return cleanup_on_error(line, file_fd);
        }
    }

    if (ferror(stdin)) {
        perror("getline");
        return cleanup_on_error(line, file_fd);
    }

    free(line);

    if (close(file_fd) == -1) {
        perror("close");
        return 1;
    }

    return 0;
}
