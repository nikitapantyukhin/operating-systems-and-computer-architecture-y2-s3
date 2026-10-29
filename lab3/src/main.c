#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    int *arr;
    int n;
    int start;
    int end;
    pthread_barrier_t *barrier;
} ThreadArgs;

int parse_positive_int(const char *text, int *result) {
    char *end;
    errno = 0;
    long value = strtol(text, &end, 10);

    if (errno == ERANGE || end == text || *end != '\0' || value < 1 || value > INT_MAX) {
        return 0;
    }

    *result = (int)value;
    return 1;
}

void *worker(void *arg) {
    ThreadArgs *task = arg;

    for (long long size = 2; size <= task->n; size *= 2) {
        for (int distance = (int)(size / 2); distance > 0; distance /= 2) {
            for (int k = task->start; k < task->end; k++) {
                int i = (k / distance) * (2 * distance) + (k % distance);
                int j = i + distance;
                int ascending = (i / size) % 2 == 0;

                if ((ascending && task->arr[i] > task->arr[j]) || (!ascending && task->arr[i] < task->arr[j])) {
                    int temp = task->arr[i];
                    task->arr[i] = task->arr[j];
                    task->arr[j] = temp;
                }
            }

            pthread_barrier_wait(task->barrier);
        }
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if ((argc != 5 && argc != 7) ||
        strcmp(argv[1], "-t") != 0 ||
        strcmp(argv[3], "-n") != 0 ||
        (argc == 7 && strcmp(argv[5], "-d") != 0)) {
        fprintf(stderr, "Usage: %s -t THREADS -n SIZE [-d TYPE]\n", argv[0]);
        return 1;
    }

    int requested_threads;
    int n;

    if (!parse_positive_int(argv[2], &requested_threads) || !parse_positive_int(argv[4], &n)) {
        fprintf(stderr, "Threads and size must be positive integers\n");
        return 1;
    }

    if (n < 2 || (n & (n - 1)) != 0) {
        fprintf(stderr, "Array size must be a power of two (at least 2)\n");
        return 1;
    }

    const char *data_type = argc == 7 ? argv[6] : "random";

    if (strcmp(data_type, "random") != 0 &&
        strcmp(data_type, "sorted") != 0 &&
        strcmp(data_type, "reverse") != 0 &&
        strcmp(data_type, "repeated") != 0) {
        fprintf(stderr, "Invalid data type (random, sorted, reverse, repeated)\n");
        return 1;
    }

    int comparisons = n / 2;
    int thread_count = requested_threads;

    if (thread_count > comparisons) {
        thread_count = comparisons;
    }

    int *arr = malloc((size_t)n * sizeof(*arr));
    ThreadArgs *tasks = malloc((size_t)thread_count * sizeof(*tasks));
    pthread_t *threads = NULL;

    if (thread_count > 1) {
        threads = malloc((size_t)(thread_count - 1) * sizeof(*threads));
    }

    if (arr == NULL || tasks == NULL ||
        (thread_count > 1 && threads == NULL)) {
        fprintf(stderr, "Memory allocation failed\n");
        free(arr);
        free(tasks);
        free(threads);
        return 1;
    }

    srand(12345);

    for (int i = 0; i < n; i++) {
        if (strcmp(data_type, "random") == 0) {
            arr[i] = rand() % 2001 - 1000;
        } else if (strcmp(data_type, "sorted") == 0) {
            arr[i] = i + 1;
        } else if (strcmp(data_type, "reverse") == 0) {
            arr[i] = n - i;
        } else {
            arr[i] = rand() % 10;
        }
    }

    pthread_barrier_t barrier;

    if (pthread_barrier_init(&barrier, NULL, (unsigned)thread_count) != 0) {
        fprintf(stderr, "Barrier initialization failed\n");
        free(arr);
        free(tasks);
        free(threads);
        return 1;
    }

    for (int t = 0; t < thread_count; t++) {
        tasks[t].arr = arr;
        tasks[t].n = n;
        tasks[t].start = (int)((size_t)t * comparisons / thread_count);
        tasks[t].end = (int)((size_t)(t + 1) * comparisons / thread_count);
        tasks[t].barrier = &barrier;
    }

    struct timespec start_time, end_time;

    if (clock_gettime(CLOCK_MONOTONIC, &start_time) != 0) {
        perror("clock_gettime");
        pthread_barrier_destroy(&barrier);
        free(arr);
        free(tasks);
        free(threads);
        return 1;
    }

    for (int t = 1; t < thread_count; t++) {
        if (pthread_create(&threads[t - 1], NULL, worker, &tasks[t]) != 0) {
            fprintf(stderr, "Thread creation failed\n");
            exit(EXIT_FAILURE);
        }
    }

    worker(&tasks[0]);

    for (int t = 0; t < thread_count - 1; t++) {
        pthread_join(threads[t], NULL);
    }

    if (clock_gettime(CLOCK_MONOTONIC, &end_time) != 0) {
        perror("clock_gettime");
        pthread_barrier_destroy(&barrier);
        free(arr);
        free(tasks);
        free(threads);
        return 1;
    }

    double elapsed = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

    pthread_barrier_destroy(&barrier);

    int sorted = 1;

    for (int i = 1; i < n; i++) {
        if (arr[i - 1] > arr[i]) {
            sorted = 0;
            break;
        }
    }

    printf("Array size: %d\n", n);
    printf("Requested threads: %d\n", requested_threads);
    printf("Used threads: %d\n", thread_count);
    printf("Data type: %s\n", data_type);
    printf("Sorted: %s\n", sorted ? "yes" : "no");
    printf("Time: %.6f s\n", elapsed);

    free(arr);
    free(tasks);
    free(threads);

    return sorted ? 0 : 1;
}
