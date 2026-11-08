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

void *worker(void *arg) {
    ThreadArgs *task = arg;

    for (long long size = 2; size <= task->n; size *= 2) {
        for (int distance = (int)(size / 2); distance > 0; distance /= 2) {
            for (int k = task->start; k < task->end; k++) {
                int i = (k / distance) * (2 * distance) + (k % distance);
                int j = i + distance;
                int ascending = (i / size) % 2 == 0;

                if ((ascending && task->arr[i] > task->arr[j]) ||
                    (!ascending && task->arr[i] < task->arr[j])) {
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

int parse_int(const char *text, int *value) {
    char *end;
    errno = 0;
    long number = strtol(text, &end, 10);

    if (text == end || *end != '\0' || errno == ERANGE || number < INT_MIN || number > INT_MAX) {
        return 0;
    }

    *value = (int)number;
    return 1;
}

int main(int argc, char *argv[]) {
    int requested_threads = 0;
    int n = 0;
    const char *data_type = "random";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            if (!parse_int(argv[++i], &requested_threads)) {
                fprintf(stderr, "Invalid thread count.\n");
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            if (!parse_int(argv[++i], &n)) {
                fprintf(stderr, "Invalid array size.\n");
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            data_type = argv[++i];
        } else {
            fprintf(stderr, "Usage: %s -t THREADS -n SIZE [-d random|sorted|reverse|repeated|manual]\n", argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (requested_threads < 1 || n < 1 || n > INT_MAX / 2) {
        fprintf(stderr, "Threads and array size must be positive (array size <= INT_MAX / 2).\n");
        return EXIT_FAILURE;
    }

    if (strcmp(data_type, "random") != 0 &&
        strcmp(data_type, "sorted") != 0 &&
        strcmp(data_type, "reverse") != 0 &&
        strcmp(data_type, "repeated") != 0 &&
        strcmp(data_type, "manual") != 0) {
        fprintf(stderr, "Unknown data type: %s\n", data_type);
        return EXIT_FAILURE;
    }

    int sort_n = 2;
    while (sort_n < n) {
        sort_n *= 2;
    }

    int *arr = malloc((size_t)sort_n * sizeof(int));
    if (arr == NULL) {
        fprintf(stderr, "Could not allocate array.\n");
        return EXIT_FAILURE;
    }

    if (strcmp(data_type, "manual") == 0) {
        printf("Enter %d integer%s:\n", n, n == 1 ? "" : "s");
        fflush(stdout);
        for (int i = 0; i < n; i++) {
            char input[64];
            if (scanf("%63s", input) != 1 || strlen(input) == 63 || !parse_int(input, &arr[i])) {
                fprintf(stderr, "Invalid integer at position %d (allowed range: %d to %d).\n", i + 1, INT_MIN, INT_MAX);
                free(arr);
                return EXIT_FAILURE;
            }
        }
    } else {
        srand(12345);
        for (int i = 0; i < n; i++) {
            if (strcmp(data_type, "random") == 0) {
                arr[i] = rand() % 100000;
            } else if (strcmp(data_type, "sorted") == 0) {
                arr[i] = i;
            } else if (strcmp(data_type, "reverse") == 0) {
                arr[i] = n - i;
            } else {
                arr[i] = rand() % 10;
            }
        }
    }

    if (n <= 32) {
        printf("\nBitonic Sort\n");
        printf("Input:  ");
        for (int i = 0; i < n; i++) {
            printf("%d%s", arr[i], i + 1 == n ? "\n" : " ");
        }
    } else {
        printf("\nBitonic Sort\n");
    }

    for (int i = n; i < sort_n; i++) {
        arr[i] = INT_MAX;
    }

    int comparisons = sort_n / 2;
    int thread_count = requested_threads < comparisons ? requested_threads : comparisons;

    ThreadArgs *tasks = malloc((size_t)thread_count * sizeof(ThreadArgs));
    pthread_t *threads = NULL;
    if (thread_count > 1) {
        threads = malloc((size_t)(thread_count - 1) * sizeof(pthread_t));
    }
    if (tasks == NULL || (thread_count > 1 && threads == NULL)) {
        fprintf(stderr, "Could not allocate thread structures.\n");
        free(threads);
        free(tasks);
        free(arr);
        return EXIT_FAILURE;
    }

    pthread_barrier_t barrier;
    if (pthread_barrier_init(&barrier, NULL, (unsigned int)thread_count) != 0) {
        fprintf(stderr, "Could not create barrier.\n");
        free(threads);
        free(tasks);
        free(arr);
        return EXIT_FAILURE;
    }

    for (int t = 0; t < thread_count; t++) {
        tasks[t].arr = arr;
        tasks[t].n = sort_n;
        tasks[t].start = (int)((size_t)t * comparisons / thread_count);
        tasks[t].end = (int)((size_t)(t + 1) * comparisons / thread_count);
        tasks[t].barrier = &barrier;
    }

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    for (int t = 1; t < thread_count; t++) {
        if (pthread_create(&threads[t - 1], NULL, worker, &tasks[t]) != 0) {
            fprintf(stderr, "Thread creation failed.\n");
            exit(EXIT_FAILURE);
        }
    }

    worker(&tasks[0]);

    for (int t = 0; t < thread_count - 1; t++) {
        pthread_join(threads[t], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &end_time);

    double elapsed = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

    int sorted = 1;
    for (int i = 1; i < n; i++) {
        if (arr[i - 1] > arr[i]) {
            sorted = 0;
            break;
        }
    }

    if (n <= 32) {
        printf("Output: ");
        for (int i = 0; i < n; i++) {
            printf("%d%s", arr[i], i + 1 == n ? "\n" : " ");
        }
        printf("\n");
    }

    printf("Elements: %d\n", n);
    printf("Sorting array size: %d\n", sort_n);
    printf("Requested threads: %d\n", requested_threads);
    printf("Used threads: %d\n", thread_count);
    printf("Data type: %s\n", data_type);
    printf("Sorted: %s\n", sorted ? "yes" : "no");
    printf("Time: %.6f s\n", elapsed);

    pthread_barrier_destroy(&barrier);
    free(threads);
    free(tasks);
    free(arr);
    return sorted ? EXIT_SUCCESS : EXIT_FAILURE;
}
