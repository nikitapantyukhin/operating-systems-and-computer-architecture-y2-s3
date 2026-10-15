#include "io_utils.h"

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int get_child_path(char *child_path, size_t child_path_size) {
    char parent_path[PATH_MAX];
    ssize_t path_length = readlink("/proc/self/exe", parent_path, sizeof(parent_path) - 1);

    if (path_length == -1) {
        perror("readlink");
        return -1;
    }

    if ((size_t)path_length >= sizeof(parent_path) - 1) {
        fprintf(stderr, "Executable path is too long\n");
        return -1;
    }

    parent_path[path_length] = '\0';
    char *last_slash = strrchr(parent_path, '/');

    if (last_slash == NULL) {
        fprintf(stderr, "Failed to determine executable directory\n");
        return -1;
    }

    *last_slash = '\0';
    int result = snprintf(child_path, child_path_size, "%s/child", parent_path);

    if (result < 0 || (size_t)result >= child_path_size) {
        fprintf(stderr, "Child executable path is too long\n");
        return -1;
    }

    return 0;
}

static int wait_for_child(pid_t pid, int *status) {
    while (waitpid(pid, status, 0) == -1) {
        if (errno == EINTR) {
            continue;
        }

        perror("waitpid");
        return -1;
    }

    return 0;
}

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
    struct sigaction sa = {0};
    sa.sa_handler = SIG_IGN;

    if (sigemptyset(&sa.sa_mask) == -1) {
        perror("sigemptyset");
        return 1;
    }

    if (sigaction(SIGPIPE, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    char *filename = NULL;
    size_t filename_size = 0;

    printf("Enter filename: ");

    if (fflush(stdout) == EOF) {
        perror("fflush");
        return 1;
    }

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

    char child_path[PATH_MAX];

    if (get_child_path(child_path, sizeof(child_path)) == -1) {
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
        close_fd(pipefd[0]);
        close_fd(pipefd[1]);
        free(filename);
        return 1;
    }

    if (pid == 0) {
        if (close_fd(pipefd[1]) == -1) {
            free(filename);
            _exit(1);
        }

        if (dup2(pipefd[0], STDIN_FILENO) == -1) {
            perror("dup2");
            close_fd(pipefd[0]);
            free(filename);
            _exit(1);
        }

        if (close_fd(pipefd[0]) == -1) {
            free(filename);
            _exit(1);
        }

        execl(child_path, "child", filename, NULL);
        perror("execl");
        free(filename);
        _exit(1);
    }

    if (close_fd(pipefd[0]) == -1) {
        close_fd(pipefd[1]);
        free(filename);

        int status;

        if (wait_for_child(pid, &status) == -1) {
            return 1;
        }

        return 1;
    }

    printf("Parent PID: %ld, Child PID: %ld\n", (long)getpid(), (long)pid);
    free(filename);

    char *line = NULL;
    size_t line_size = 0;
    ssize_t line_length;
    int parent_error = 0;

    printf("Enter numbers line by line. Press Ctrl+D to finish.\n");

    if (fflush(stdout) == EOF) {
        perror("fflush");
        parent_error = 1;
    }

    if (!parent_error) {
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
    }

    free(line);

    if (close_fd(pipefd[1]) == -1) {
        parent_error = 1;
    }

    int status;

    if (wait_for_child(pid, &status) == -1) {
        return 1;
    }

    if (check_child_status(status) == -1) {
        parent_error = 1;
    }

    return parent_error ? 1 : 0;
}
