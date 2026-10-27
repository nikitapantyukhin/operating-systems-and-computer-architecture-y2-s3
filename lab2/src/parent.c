#include "io_utils.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int check_child_status(int status) {
    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);

        if (exit_code == 0) {
            return 0;
        }

        fprintf(stderr, "Child exited with code %d\n", exit_code);
        return -1;
    }

    if (WIFSIGNALED(status)) {
        fprintf(stderr, "Child was terminated by signal %d\n", WTERMSIG(status));
        return -1;
    }

    fprintf(stderr, "Child terminated unexpectedly\n");
    return -1;
}

int main(void) {
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("signal");
        return 1;
    }

    char *filename = NULL;
    size_t filename_size = 0;

    printf("Enter filename: ");

    fflush(stdout);

    ssize_t filename_length = getline(&filename, &filename_size, stdin);

    if (filename_length == -1) {
        if (feof(stdin)) {
            fprintf(stderr, "Filename was not provided\n");
        } else {
            perror("getline");
        }

        free(filename);
        return 1;
    }

    filename[strcspn(filename, "\r\n")] = '\0';

    if (filename[0] == '\0') {
        fprintf(stderr, "Filename cannot be empty\n");
        free(filename);
        return 1;
    }

    int pipefd[2];

    if (pipe(pipefd) == -1) {
        perror("pipe");
        free(filename);
        return 1;
    }

    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        close(pipefd[0]);
        close(pipefd[1]);
        free(filename);
        return 1;
    }

    if (pid == 0) {
        close(pipefd[1]);

        if (dup2(pipefd[0], STDIN_FILENO) == -1) {
            perror("dup2");
            close(pipefd[0]);
            free(filename);
            _exit(1);
        }

        close(pipefd[0]);

        execl("./child", "child", filename, NULL);
        perror("execl");
        free(filename);
        _exit(1);
    }

    close(pipefd[0]);

    printf("Parent PID: %ld, Child PID: %ld\n", (long)getpid(), (long)pid);
    free(filename);

    char *line = NULL;
    size_t line_size = 0;
    ssize_t line_length;
    int parent_error = 0;

    printf("Enter numbers line by line. Press Ctrl+D to finish.\n");

    fflush(stdout);

    while ((line_length = getline(&line, &line_size, stdin)) != -1) {
        if (write_all(pipefd[1], line, (size_t)line_length) == -1) {
            parent_error = 1;
            break;
        }
    }

    if (ferror(stdin)) {
        perror("getline");
        parent_error = 1;
    }

    free(line);
    close(pipefd[1]);

    int status;

    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        return 1;
    }

    if (check_child_status(status) == -1) {
        parent_error = 1;
    }

    return parent_error ? 1 : 0;
}
