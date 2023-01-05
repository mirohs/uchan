/*
UChan is an unbounded first-in-first-out communication channel between multiple
threads. UChan is implemented using pthreads [pthreads Wikipedia][pthreads
OpenGroup]. It differs from Go channels [Go channels] in that it does not have a
fixed capacity.

Guarantees of the channel API:
- FIFO behavior: order sent (from the same thread) equals order received
- no order specification for sends from different threads (*);
- for closed channels: channel will be drained, i.e., all values sent so far
  (and still in the channel) can still be received;
- reading from a closed and drained channel returns NULL (zero value) without
  blocking
- closing a closed channel is an error;
- sending on a closed channel is an error.

(*) FIFO order is not guaranteed by pthread_cond_wait, i.e., threads may not be
unblocked in the order in which they called pthread_cond_wait.

[Go channels]: https://go.dev/ref/spec#UChannel_types
[pthreads Wikipedia]: https://en.wikipedia.org/wiki/Pthreads
[pthreads OpenGroup]: https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/pthread.h.html
[pthreads macOS]: man pthread

@author: Michael Rohs
@date: January 5, 2023
*/

#include <pthread.h>
#include "uchan.h"
#include "vqueue.h"

#define DEBUG_LOG //stderr_log("q=%d, s=%d, r=%d", queue_len(ch->queue), ch->n_waiting_senders, ch->n_waiting_receivers);

// Required for casts between void* and long int.
_Static_assert(sizeof(void*) == sizeof(long int), "valid size");

// A channel-select thread sends or receives one item to/from a channel. Such
// threads may be cancelled if another thread was selected. Information about the
// item that is processed by the thread is stored as thread-specific data
// (thread-local storage):
pthread_key_t key_chan_select_item;
typedef struct UChanSelectItem UChanSelectItem;
bool uchan_select_continue(UChanSelectItem* csi);

struct UChan {
    pthread_mutex_t mutex;
    pthread_cond_t waiting_receivers;
    VQueue* queue;
    bool closed;
    int n_waiting_receivers;
};

// Creates a channel.
UChan* uchan_new(void) {
    int error;

    if (key_chan_select_item == 0) {
        error = pthread_key_create(&key_chan_select_item, NULL);
        panic_if(error != 0, "error %d", error);
    }

    UChan* ch = xcalloc(1, sizeof(UChan));

    error = pthread_mutex_init(&ch->mutex, NULL);
    panic_if(error != 0, "error %d", error);

    error = pthread_cond_init(&ch->waiting_receivers, NULL);
    panic_if(error != 0, "error %d", error);

    ch->queue = vqueue_new();

    return ch;
}

// Frees the resources associated with this channel.
void uchan_free(UChan* ch) {
    require_not_null(ch);
    if (!ch->closed) {
        uchan_close(ch);
    }
    int error = pthread_cond_destroy(&ch->waiting_receivers);
    panic_if(error != 0, "error %d", error);
    error = pthread_mutex_destroy(&ch->mutex);
    panic_if(error != 0, "error %d", error);
    vqueue_free(ch->queue);
    free(ch);
}

// Sends x to the given channel. x is allowed to be NULL.
// Panics if the channel is already closed.
void uchan_send(UChan* ch, /*in*/void* x) {
    require_not_null(ch);

    int error = pthread_mutex_lock(&ch->mutex);
    panic_if(error != 0, "error %d", error);

    panic_if(ch->closed, "send on closed channel");

#if 0
    DEBUG_LOG
    UChanSelectItem* item = pthread_getspecific(key_chan_select_item);
    // stderr_log("key = %lu, item = %p", key_chan_select_item, item);
    if (item != NULL && !uchan_select_continue(item)) {
        DEBUG_LOG
        /*
        error = pthread_cond_broadcast(&ch->waiting_receivers);
        panic_if(error != 0, "error %d", error);
        */
        error = pthread_mutex_unlock(&ch->mutex);
        panic_if(error != 0, "error %d", error);
        pthread_exit(NULL);
        assert("not reached", false);
    }
    DEBUG_LOG
#endif

    vqueue_put(ch->queue, x);

    error = pthread_cond_broadcast(&ch->waiting_receivers);
    panic_if(error != 0, "error %d", error);

    error = pthread_mutex_unlock(&ch->mutex);
    panic_if(error != 0, "error %d", error);
}

// Receives a value from the channel and writes it to x. Blocks until a value is
// available. If the channel is already closed, returns values that are still in
// the channel. If the channel is closed and there are no more values in the
// channel, writes NULL to x. Returns true iff there was a value in the channel.
// (*x == NULL is not an indication of the end of the channel, because NULL values
// are allowed.)
bool uchan_receive2(UChan* ch, /*out*/void** x) {
    require_not_null(ch);
    require_not_null(x);

    int error = pthread_mutex_lock(&ch->mutex);
    panic_if(error != 0, "error %d", error);

    DEBUG_LOG
    while (vqueue_empty(ch->queue) && !ch->closed) {
        ch->n_waiting_receivers++;
        DEBUG_LOG
        error = pthread_cond_wait(&ch->waiting_receivers, &ch->mutex);
        panic_if(error != 0, "error %d", error);
        ch->n_waiting_receivers--;
        DEBUG_LOG
    }

#if 0
    DEBUG_LOG
    UChanSelectItem* item = pthread_getspecific(key_chan_select_item);
    // stderr_log("key = %lu, item = %p", key_chan_select_item, item);
    if (item != NULL && !uchan_select_continue(item)) {
        DEBUG_LOG
        error = pthread_mutex_unlock(&ch->mutex);
        panic_if(error != 0, "error %d", error);
        pthread_exit(NULL);
        assert("not reached", false);
    }
#endif

    DEBUG_LOG
    assert("not closed implies queue not empty", !vqueue_empty(ch->queue) || ch->closed);
    bool has_value = !vqueue_empty(ch->queue);
    if (has_value) {
        *x = vqueue_get(ch->queue);
    } else {
        *x = NULL;
    }

    error = pthread_mutex_unlock(&ch->mutex);
    panic_if(error != 0, "error %d", error);

    return has_value;
}

// Receives a value from the channel. Blocks until a value is available. If the
// channel is already closed, returns values that are still in the channel. If the
// channel is closed and there are no more values in the channel, returns NULL.
void* uchan_receive(UChan* ch) {
    void* x;
    uchan_receive2(ch, &x);
    return x;
}

// Sends x to the given channel.
// Panics if the channel is already closed.
void uchan_send_int(UChan* ch, int x) {
    require_not_null(ch);
    void* p = (void*)(long int)x;
    uchan_send(ch, p);
}

// Receives a value from the channel. Blocks until a value is available. If the
// channel is already closed, returns values that are still in the channel. If the
// channel is closed and there are no more values in the channel, returns 0.
int uchan_receive_int(UChan* ch) {
    require_not_null(ch);
    void* p = uchan_receive(ch);
    long int x = (long int)p;
    return (int)x;
}

// Receives a value from the channel and writes it to x. Blocks until a value is
// available. If the channel is already closed, returns values that are still in
// the channel. If the channel is closed and there are no more values in the
// channel, writes 0 to x. Returns true iff there was a value in the channel.
bool uchan_receive2_int(UChan* ch, /*out*/int* x) {
    require_not_null(ch);
    void* p = NULL;
    bool has_value = uchan_receive2(ch, &p);
    if (has_value) {
        long int lx = (long int)p;
        *x = (int)lx;
    } else {
        *x = 0;
    }
    return has_value;
}

// Closes the given channel. Panics if the channel was already closed.
void uchan_close(UChan* ch) {
    require_not_null(ch);

    int error = pthread_mutex_lock(&ch->mutex);
    panic_if(error != 0, "error %d", error);

    panic_if(ch->closed, "close of closed channel");
    ch->closed = true;

    error = pthread_cond_broadcast(&ch->waiting_receivers);
    panic_if(error != 0, "error %d", error);

    error = pthread_mutex_unlock(&ch->mutex);
    panic_if(error != 0, "error %d", error);
}

// Returns the number of values that are currently in the channel.
int uchan_len(UChan* ch) {
    require_not_null(ch);

    int error = pthread_mutex_lock(&ch->mutex);
    panic_if(error != 0, "error %d", error);

    int len = vqueue_len(ch->queue);

    error = pthread_mutex_unlock(&ch->mutex);
    panic_if(error != 0, "error %d", error);

    return len;
}

// Receives a value from the channel and writes it to x. Does not wait for a value,
// but returns false if no value is currently available. If false is returned this
// means that no value is currently available. It does not mean that the channel
// has been closed (but if may have been closed). If no value is available, writes NULL
// to *x.
bool uchan_receive2_noblock(UChan* ch, /*out*/void** x) {
    require_not_null(ch);
    require_not_null(x);

    int error = pthread_mutex_lock(&ch->mutex);
    panic_if(error != 0, "error %d", error);

    bool has_value = !vqueue_empty(ch->queue);
    if (has_value) {
        *x = vqueue_get(ch->queue);
    } else {
        *x = NULL;
    }

    error = pthread_mutex_unlock(&ch->mutex);
    panic_if(error != 0, "error %d", error);

    return has_value;
}

#if 0
typedef struct UChanSelectItem UChanSelectItem;
struct UChanSelectItem {
    UChanSelect* cs;
    UChan* ch;
    void* x;
    bool* has_value;
    pthread_t thread;
    UChanSelectItem* next;
};

struct UChanSelect {
    UChanSelectItem* items;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    UChanSelectItem* selected;
};

UChanSelect* uchan_select_new(void) {
    UChanSelect* cs = xcalloc(1, sizeof(UChanSelect));
    int error;

    error = pthread_mutex_init(&cs->mutex, NULL);
    panic_if(error != 0, "error %d", error);

    error = pthread_cond_init(&cs->cond, NULL);
    panic_if(error != 0, "error %d", error);

    return cs;
}

void uchan_select_free(UChanSelect* cs) {
    require_not_null(cs);
    UChanSelectItem* item_next;
    for (UChanSelectItem* item = cs->items; item; item = item_next) {
        item_next = item->next;
        // todo: destroy thread
        free(item);
    }
    // todo: destroy mutex and cond
    free(cs);
}

static void permute_indices(int* indices, int n) {
    for (int i = 0; i < n; i++) {
        indices[i] = i;
    }
    for (int i = n - 1; i >= 0; i--) {
        int j = i_rnd(i + 1);
        int h = indices[i];
        indices[i] = indices[j];
        indices[j] = h;
    }
}

UChan* uchan_select_noblock(UChanSelect* cs) {
    require_not_null(cs);
    int n = 0;
    for (UChanSelectItem* item = cs->items; item; item = item->next) {
        n++;
    }
    UChanSelectItem* items[n];
    int i = 0;
    for (UChanSelectItem* item = cs->items; item; item = item->next) {
        items[i++] = item;
    }
    int indices[n];
    permute_indices(indices, n);

    for (i = 0; i < n; i++) {
        UChanSelectItem* item = items[indices[i]];
        UChan* ch = item->ch;
        if (uchan_receive2_noblock(ch, item->x)) return ch;
    }
    // assert: none of the channels was ready for communication
    return NULL;
}

void cleanup_select_thread(void* arg) {
    UChanSelectItem* item = arg;
    stderr_log("item = %p", item);
    int error = pthread_mutex_unlock(&item->ch->mutex);
    panic_if(error != 0, "error %d", error);
}

void* uchan_select_item_func(void* arg) {
    UChanSelectItem* item = arg;
    int error = pthread_setspecific(key_chan_select_item, item);
    panic_if(error != 0, "error %d", error);
    pthread_cleanup_push(cleanup_select_thread, item);
    *item->has_value = uchan_receive2(item->ch, item->x);
    stderr_log("received: %d, ok = %d", (int)*(long int*)item->x, *item->has_value);
    // signal select thread that a response is available
    error = pthread_cond_signal(&item->cs->cond);
    panic_if(error != 0, "error %d", error);
    pthread_cleanup_pop(0);
    return NULL;
}

UChan* uchan_select(UChanSelect* cs) {
    require_not_null(cs);

    // try non-blocking communication first
    UChan* ch = NULL;//uchan_select_noblock(cs);
    stderr_log("noblock ch = %p", ch);
    if (ch != NULL) return ch;

    // communicate with the channels using separate threads
    for (UChanSelectItem* item = cs->items; item; item = item->next) {
        int error = pthread_create(&item->thread, NULL, uchan_select_item_func, item);
        panic_if(error != 0, "error %d", error);
    }

    // then wait
    int error = pthread_mutex_lock(&cs->mutex);
    panic_if(error != 0, "error %d", error);

    stderr_log("");

    error = pthread_cond_wait(&cs->cond, &cs->mutex);
    panic_if(error != 0, "error %d", error);
    assert("selection done", cs->selected != NULL);

    stderr_log("sel = %p, x = %d", cs->selected, (int)(long int)cs->selected->x);

    /*
    // done, cancel threads
    for (UChanSelectItem* item = cs->items; item; item = item->next) {
        if (item != cs->selected && item->thread != NULL) {
            error = pthread_cancel(item->thread);
            panic_if(error != 0, "error %d", error);
            // todo: have to release mutex of item->thread?
        }
        // todo: delete item?
    }
    */

    error = pthread_mutex_unlock(&cs->mutex);
    panic_if(error != 0, "error %d", error);

    UChanSelectItem* selected = cs->selected;
    // todo: delete item?
    return selected->ch;
}

// Checks whether csi is the selected channel operation, i.e. whether it should
// actually be executed.
bool uchan_select_continue(UChanSelectItem* csi) {
    UChanSelect* cs = csi->cs;
    bool result = false;
    int error = pthread_mutex_lock(&cs->mutex);
    panic_if(error != 0, "error %d", error);
    if (cs->selected == NULL) {
        cs->selected = csi;
        result = true;
        // cancel all other select threads
        for (UChanSelectItem* item = cs->items; item; item = item->next) {
            stderr_log("sel = %p, item = %p", cs->selected, item);
            if (item != csi) {
                error = pthread_cancel(item->thread);
                panic_if(error != 0, "error %d", error);
                // todo: have to release mutex of item->thread?
            }
        }
    }
    error = pthread_mutex_unlock(&cs->mutex);
    panic_if(error != 0, "error %d", error);
    return result;
}

// Registers a receive on channel ch for a subsequent select operation. The
// result is written to *x. *has_value indicates whether a value was received.
// has_value is allowed to be be NULL. If no value is available, e.g. because
// the channel has been closed, x is set to NULL.
void uchan_select_receive(UChanSelect* cs, UChan* ch, /*out*/void** x, /*out*/bool* has_value) {
    require_not_null(cs);
    require_not_null(ch);
    require_not_null(x);
    UChanSelectItem* i = xcalloc(1, sizeof(UChanSelectItem));
    i->cs = cs;
    i->ch = ch;
    i->x = x;
    i->has_value = has_value;
    i->next = cs->items;
    cs->items = i;
}

// Registers a receive on channel ch for a subsequent select operation. The
// result is written to *x. *has_value indicates whether a value was received.
// has_value is allowed to be be NULL. If no value is available, e.g. because
// the channel has been closed, x is set to NULL.
void uchan_select_receive_int(UChanSelect* cs, UChan* ch, /*out*/int* x, /*out*/bool* has_value) {
    require_not_null(cs);
    require_not_null(ch);
    require_not_null(x);
    uchan_select_receive(cs, ch, (void**)x, has_value);
}
#endif
