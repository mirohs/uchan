/*
Countdown allows the coordination of multiple threads. Usage is as follows. The
countdown is initialized to some positive value. The countdown object is
provided to the relevant threads. The threads then decrement the counter (as
they complete work). Other threads may wait on the countdown and get signalled
when the countdown reaches zero or gets negative.

@author: Michael Rohs
@date: January 5, 2023
*/

#include <pthread.h>
#include <stdatomic.h>
#include "countdown.h"

struct Countdown {
    atomic_int n;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
};

Countdown* countdown_new(int n) {
    require("positive", n > 0);
    Countdown* c = xmalloc(sizeof(Countdown));
    atomic_store(&c->n, n);
    int error = pthread_mutex_init(&c->mutex, NULL);
    panic_if(error != 0, "error %d", error);
    error = pthread_cond_init(&c->cond, NULL);
    panic_if(error != 0, "error %d", error);
    return c;
}

void countdown_add(Countdown* c, int i) {
    require_not_null(c);
    int n = atomic_fetch_add(&c->n, i);
    if (n + i <= 0) {
        int error = pthread_cond_broadcast(&c->cond);
        panic_if(error != 0, "error %d", error);
    }
}

void countdown_inc(Countdown* c) {
    require_not_null(c);
    int n = atomic_fetch_add(&c->n, 1);
    if (n + 1 <= 0) {
        int error = pthread_cond_broadcast(&c->cond);
        panic_if(error != 0, "error %d", error);
    }
}

void countdown_sub(Countdown* c, int i) {
    require_not_null(c);
    int n = atomic_fetch_sub(&c->n, i);
    if (n - i <= 0) {
        int error = pthread_cond_broadcast(&c->cond);
        panic_if(error != 0, "error %d", error);
    }
}

void countdown_dec(Countdown* c) {
    require_not_null(c);
    int n = atomic_fetch_sub(&c->n, 1);
    if (n - 1 <= 0) {
        int error = pthread_cond_broadcast(&c->cond);
        panic_if(error != 0, "error %d", error);
    }
}

// Blocks the calling thread until the countdown reaches zero.
void countdown_wait(Countdown* c) {
    require_not_null(c);
    int error = pthread_mutex_lock(&c->mutex);
    panic_if(error != 0, "error %d", error);
    while (atomic_load(&c->n) > 0) {
        int error = pthread_cond_wait(&c->cond, &c->mutex);
        panic_if(error != 0, "error %d", error);
    }
    error = pthread_mutex_unlock(&c->mutex);
    panic_if(error != 0, "error %d", error);
}

// Unblocks any waiting threads (even if the counter has not reached zero) and
// releases the resources that are associated with this object.
void countdown_free(Countdown* c) {
    require_not_null(c);
    int error = pthread_cond_broadcast(&c->cond);
    panic_if(error != 0, "error %d", error);
    error = pthread_cond_destroy(&c->cond);
    panic_if(error != 0, "error %d", error);
    error = pthread_mutex_destroy(&c->mutex);
    panic_if(error != 0, "error %d", error);
    free(c);
}

void countdown_set(Countdown* c, int i) {
    require_not_null(c);
    atomic_store(&c->n, i);
    if (i <= 0) {
        int error = pthread_cond_broadcast(&c->cond);
        panic_if(error != 0, "error %d", error);
    }
}

int countdown_get(Countdown* c) {
    require_not_null(c);
    return atomic_load(&c->n);
}

bool countdown_finished(Countdown* c) {
    require_not_null(c);
    return atomic_load(&c->n) <= 0;
}
