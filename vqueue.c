/*
VQueue is a vector-based queue that automatically grows and shrinks as necessary.

@author: Michael Rohs
@date: January 5, 2023
*/

#if 0
#define NO_ASSERT
#define NO_REQUIRE
#define NO_ENSURE
#endif

#include "vqueue.h"

// Initial capacity (number of items).
#define INITIAL_CAP 512

struct VQueue {
    int cap; // capacity of the data array (maximum number of items)
    int len; // number of items in the queue
    int head; // next position to read
    int tail; // next position to write
    void** data; // points to an array of void*
};

VQueue* vqueue_new(void) {
    VQueue* q = xcalloc(1, sizeof(VQueue));
    q->cap = INITIAL_CAP;
    q->data = xmalloc(INITIAL_CAP * sizeof(void*));
    return q;
}

void vqueue_free(VQueue* q) {
    require_not_null(q);
    free(q->data);
    free(q);
}

// Enqueues x in q. x == NULL is allowed.
void vqueue_put(VQueue* q, void* x) {
    require_not_null(q);
    if (q->len == q->cap) {
        int n = q->cap;
        int h = q->head;
        int t = q->tail;
        // stderr_log("cap=%d h=%d t=%d", n, h, t);
        assert("head index equals tail index", h == t);
        int cap_new = 2 * n;
        void** new = xmalloc(cap_new * sizeof(void*));
        // |*******************|
        //        h            n
        //        t            n
        memcpy(new, q->data + h, (n - h) * sizeof(void*));
        memcpy(new + (n - h), q->data, t * sizeof(void*));
        free(q->data);
        q->cap = cap_new;
        q->head = 0;
        q->tail = n;
        q->data = new;
    }
    q->data[q->tail] = x;
    q->len++;
    q->tail = (q->tail + 1) % q->cap;
}

// Dequeues and returns a value from q. Q must not be empty.
void* vqueue_get(VQueue* q) {
    require_not_null(q);
    require("not empty", !vqueue_empty(q));
    void* x = q->data[q->head];
    q->len--;
    q->head = (q->head + 1) % q->cap;
    if (q->cap > INITIAL_CAP && q->len < q->cap / 4) {
        int n = q->cap;
        int h = q->head;
        int t = q->tail;
        // stderr_log("cap=%d len=%d h=%d t=%d", n, q->len, h, t);
        int cap_new = n / 2;
        if (cap_new < INITIAL_CAP) cap_new = INITIAL_CAP;
        void** new = xmalloc(cap_new * sizeof(void*));
        if (h <= t) {
            // |---********--------|
            //     h       t       n
            memcpy(new, q->data + h, (t - h) * sizeof(void*));
        } else {
            // |******        *****|
            //        t       h    n
            memcpy(new, q->data + h, (n - h) * sizeof(void*));
            memcpy(new + (n - h), q->data, t * sizeof(void*));
        }
        free(q->data);
        q->cap = cap_new;
        q->head = 0;
        q->tail = q->len;
        q->data = new;
    }
    return x;
}

// Checks whether q is empty.
bool vqueue_empty(VQueue* q) {
    require_not_null(q);
    return q->len <= 0;
}

// Returns the number of items in q.
int vqueue_len(VQueue* q) {
    require_not_null(q);
    return q->len;
}

