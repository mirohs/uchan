/*
UChan is an unbounded first-in-first-out communication channel for communication
between multiple threads. UChan is implemented using pthreads [pthreads
Wikipedia][pthreads OpenGroup]. It differs from Go channels [Go channels] in
that it does not have a fixed capacity.

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
#include <stdatomic.h>
#include "uchan.h"
#include "vqueue.h"

// Required for casts between void* and long int.
_Static_assert(sizeof(void*) == sizeof(long int), "valid size");

// A channel-select thread receives one item from a channel. Such threads may be
// cancelled if another channel was selected. Information about the item that is
// processed by the thread is stored as thread-specific data (thread-local
// storage):
pthread_key_t key_chan_select_item;
typedef struct UChanSelectItem UChanSelectItem;
static void check_select_continue(UChanSelectItem* item);

struct UChan {
    pthread_mutex_t mutex;
    pthread_cond_t waiting_receivers;
    VQueue* queue;
    bool closed;
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

    while (vqueue_empty(ch->queue) && !ch->closed) {
        error = pthread_cond_wait(&ch->waiting_receivers, &ch->mutex);
        panic_if(error != 0, "error %d", error);
    }

    UChanSelectItem* item = pthread_getspecific(key_chan_select_item);
    if (item != NULL) check_select_continue(item);

    assert("not closed implies queue not empty", ch->closed || !vqueue_empty(ch->queue));
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

// The rest of this file implements support for channel selection when receiving.

typedef struct UChanSelect UChanSelect;
typedef struct UChanSelectItem UChanSelectItem;

struct UChanSelect {
    UChan** channels;
    int n_channels;
    atomic_int remaining_threads;
    UChanSelectItem* items;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    UChanSelectItem* selected;
};

struct UChanSelectItem {
    UChanSelect* cs;
    UChan* ch;
    int index;
    bool has_value;
    void* x;
    pthread_t thread;
};

static int uchan_select_noblock(UChan** channels, int n_channels, void** x, bool* has_value) {
    require_not_null(channels);
    require("positive", n_channels > 0);
    require_not_null(x);

    int indices[n_channels];
    permute_indices(indices, n_channels);

    for (int i = 0; i < n_channels; i++) {
        UChan* ch = channels[indices[i]];
        bool ok = uchan_receive2_noblock(ch, x);
        if (has_value != NULL) *has_value = ok;
        if (ok) return i;
    }
    return -1;
}

static void signal_if_last_select_thread(UChanSelect* cs) {
    require_not_null(cs);
    int n = atomic_fetch_sub(&cs->remaining_threads, 1);
    stderr_log("%d remaining threads ", n);
    if (n <= 1) {
        int error = pthread_cond_signal(&cs->cond);
        panic_if(error != 0, "error %d", error);
    }
}

static void select_thread_cleanup(void* arg) {
    require_not_null(arg);
    UChanSelectItem* item = arg;
    stderr_log("cleanup item %d", item->index);
    int error = pthread_mutex_unlock(&item->ch->mutex);
    panic_if(error != 0, "error %d", error);
    signal_if_last_select_thread(item->cs);
}

static void* select_thread_func(void* arg) {
    require_not_null(arg);
    UChanSelectItem* item = arg;
    int error = pthread_setspecific(key_chan_select_item, item);
    panic_if(error != 0, "error %d", error);
    pthread_cleanup_push(select_thread_cleanup, item);

    item->has_value = uchan_receive2(item->ch, &item->x);
    stderr_log("from ch %d received: %d, ok = %d", item->index, (int)(long int)item->x, item->has_value);

    pthread_cleanup_pop(1);
    return NULL;
}

int uchan_select(UChan** channels, int n_channels, void** x, bool* has_value) {
    require_not_null(channels);
    require("positive", n_channels > 0);
    require_not_null(x);

    // try non-blocking receive first
    int i_selected = uchan_select_noblock(channels, n_channels, x, has_value);
    stderr_log("noblock ch = %d", i_selected);
    if (i_selected >= 0) return i_selected;

    UChanSelect cs = {0};
    cs.channels = channels;
    cs.n_channels = n_channels;
    atomic_store(&cs.remaining_threads, n_channels);
    UChanSelectItem items[n_channels];
    cs.items = items;
    for (int i = 0; i < n_channels; i++) {
        UChanSelectItem* item = cs.items + i;
        item->cs = &cs;
        item->ch = channels[i];
        item->index = i;
        item->has_value = false;
        item->x = NULL;
    }

    int error = pthread_mutex_init(&cs.mutex, NULL);
    panic_if(error != 0, "error %d", error);
    error = pthread_cond_init(&cs.cond, NULL);
    panic_if(error != 0, "error %d", error);

    // receive from the channels using separate threads
    for (int i = 0; i < n_channels; i++) {
        UChanSelectItem* item = cs.items + i;
        int error = pthread_create(&item->thread, NULL, select_thread_func, item);
        panic_if(error != 0, "error %d", error);
    }

    // then wait
    error = pthread_mutex_lock(&cs.mutex);
    panic_if(error != 0, "error %d", error);

    stderr_log("waiting");
    error = pthread_cond_wait(&cs.cond, &cs.mutex);
    panic_if(error != 0, "error %d", error);
    assert("selection done", cs.selected != NULL);
    stderr_log("waiting completed");

    UChanSelectItem* item = cs.selected;
    stderr_log("selected channel: %d, x = %d, ok = %d", item->index, (int)(long int)item->x, item->has_value);

    if (has_value != NULL) {
        *has_value = item->has_value;
    }
    *x = item->x;
    i_selected = item->index;

    error = pthread_mutex_unlock(&cs.mutex);
    panic_if(error != 0, "error %d", error);
    error = pthread_cond_destroy(&cs.cond);
    panic_if(error != 0, "error %d", error);
    error = pthread_mutex_destroy(&cs.mutex);
    panic_if(error != 0, "error %d", error);

    return i_selected;
}

// Checks whether csi is the selected channel operation, i.e. whether it should
// actually be executed.
static void check_select_continue(UChanSelectItem* requesting) {
    UChanSelect* cs = requesting->cs;
    bool is_selected = false;
    int error = pthread_mutex_lock(&cs->mutex);
    panic_if(error != 0, "error %d", error);
    if (cs->selected == NULL) {
        cs->selected = requesting;
        is_selected = true;
        stderr_log("selected: %d", cs->selected->index);
        // cancel all other select threads
        for (int i = 0; i < cs->n_channels; i++) {
            UChanSelectItem* item = cs->items + i;
            if (item != requesting) {
                error = pthread_cancel(item->thread);
                panic_if(error != 0, "error %d", error);
            }
        }
    } else {
        stderr_log("not selected: %d", requesting->index);
    }
    error = pthread_mutex_unlock(&cs->mutex);
    panic_if(error != 0, "error %d", error);
    if (!is_selected) {
        pthread_exit(NULL);
        assert("not reached", false);
    }
}
