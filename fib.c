#include <unistd.h>
#include <stdatomic.h>
#include "uchan.h"

_Static_assert(sizeof(void*) == sizeof(pthread_t), "valid size");

typedef struct {
    UChan* ch;
    int i;
} IntArg;

void* produce_tasks(void* arg) {
    IntArg* a = arg;
    for (int i = 0; i < a->i; i++) {
        int x = 37;
        stderr_log("producing task: %d", x);
        uchan_send_int(a->ch, x);
        // sleep(1);
    }
    uchan_close(a->ch);
    return NULL;
}

int fib(int n) {
    if (n <= 0) return 0;
    if (n == 1) return 1;
    return fib(n - 1) + fib(n - 2);
}

typedef struct {
    UChan* ch1;
    UChan* ch2;
    atomic_int countdown;
} TwoChanArg;

void* solve_tasks(void* arg) {
    TwoChanArg* a = arg;
    int x;
    while (uchan_receive2_int(a->ch1, &x)) {
        stderr_log("computing fib(%d)", x);
        uchan_send_int(a->ch2, fib(x));
    }
    if (atomic_fetch_sub(&a->countdown, 1) == 1) {
        uchan_close(a->ch2);
    }
    return NULL;
}

pthread_t run(void* (*f)(void*), void* arg) {
    pthread_t thread;
    int error = pthread_create(&thread, NULL, f, arg);
    panic_if(error != 0, "error %d", error);
    return thread;
}

void join(pthread_t thread) {
    int error = pthread_join(thread, NULL);
    panic_if(error != 0, "error %d", error);
}

void join_all(pthread_t* threads, int n_threads) {
    require_not_null(threads);
    for (int i = 0; i < n_threads; i++) {
        int error = pthread_join(threads[i], NULL);
        panic_if(error != 0, "error %d", error);
    }
}

int main(void) {
    UChan* ch_tasks = uchan_new();
    pthread_t producer = run(produce_tasks, &(IntArg){ch_tasks, 10});

    timespec start = time_now();

    UChan* ch_solutions = uchan_new();
    int n_solvers = 10;
    TwoChanArg solver_arg = {ch_tasks, ch_solutions, n_solvers};
    pthread_t solvers[n_solvers];
    for (int i = 0; i < n_solvers; i++) {
        solvers[i] = run(solve_tasks, &solver_arg);
    }

    int x;
    while (uchan_receive2_int(ch_solutions, &x)) {
        stderr_log("%d", x);
    }
    double ms = time_ms_since(start);
    stderr_log("%.1f ms", ms);

    join(producer);
    join_all(solvers, n_solvers);
    uchan_free(ch_tasks);
    uchan_free(ch_solutions);

    stderr_log("main end");
    return 0;
}

