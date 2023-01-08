#include <unistd.h>
#include "uchan.h"

_Static_assert(sizeof(void*) == sizeof(pthread_t), "valid size");

typedef struct {
    UChan* ch;
    int i;
} IntArg;

IntArg* int_arg(UChan* ch, int i) {
    IntArg* a = xcalloc(1, sizeof(IntArg));
    a->ch = ch;
    a->i = i;
    return a;
}

void* send_1_to_n(void* arg) {
    IntArg* a = arg;
    for (int i = 1; i <= a->i; i++) {
        uchan_send_int(a->ch, i);
    }
    return NULL;
}

void* receive_and_print_n_ints(void* arg) {
    IntArg* a = arg;
    int x;
    for (int i = 1; i <= a->i; i++) {
        bool ok = uchan_receive2_int(a->ch, &x);
        stderr_log("%d. x = %d, ok = %d", i, x, ok);
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
    int error;
    UChan* ch = uchan_new();
    pthread_t thread = run(send_1_to_n, int_arg(ch, 3));
    int i = uchan_receive_int(ch);
    stderr_log("%d", i);
    i = uchan_receive_int(ch);
    stderr_log("%d", i);
    i = uchan_receive_int(ch);
    stderr_log("%d", i);

    uchan_close(ch);
    i = uchan_receive_int(ch);
    stderr_log("%d", i);
    uchan_free(ch);

    error = pthread_join(thread, NULL);
    panic_if(error != 0, "error %d", error);

    // free channel with waiting readers

    ch = uchan_new();
    thread = run(receive_and_print_n_ints, int_arg(ch, 2));
    uchan_send_int(ch, 100);
    //uchan_close(ch);
    sleep(1);
    uchan_free(ch);
    //uchan_send_int(ch, 200);

    join(thread);
    // join_all(&thread, 1);
    // join_all((pthread_t[]){thread}, 1);

    stderr_log("main end");
    return 0;
}
