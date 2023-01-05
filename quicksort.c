/*
This is a multithreaded non-recursive version of Quicksort that uses unbounded
FIFO channels for communication. One step of the algorithm involves taking an
interval out of the work channel, partitioning the corresponding slice of the
array to be sorted, and putting the resulting subintervals into the channel.
Partitioning the interval means randomly picking an element of the slice as a
pivot element p and rearranging the values such that the items left of the pivot
element are less than or equal to p and the items to the right of the pivot
element are greater than p.

The algorithm works in-place in the input array. This means that the speedup
from multiple threads is not large, likely because of caching issues. To
generate a second kind of load, which only uses the local stack of each thread,
Fibonacci sequences are computed. The Fibonacci computation can be switched off
by commenting out the ENABLE_FIB symbol.

@author: Michael Rohs
@date: January 5, 2023
*/

#if 0
#define NO_ASSERT
#define NO_REQUIRE
#define NO_ENSURE
#endif

#define ENABLE_FIB
#define ARR_LENGTH 1000
#define N_THREADS 8

#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>
#include "util.h"
#include "uchan.h"
#include "countdown.h"

// Prints the stack size of the calling thread.
size_t get_stacksize(void) {
    pthread_attr_t attr;
    int error = pthread_attr_init(&attr);
    panic_if(error != 0, "error %d", error);
    size_t stacksize;
    error = pthread_attr_getstacksize(&attr, &stacksize);
    panic_if(error != 0, "error %d", error);
    error = pthread_attr_destroy(&attr);
    panic_if(error != 0, "error %d", error);
    return stacksize;
}

// Partitions the slice of array a denoted by index interval [low, high] (bounds
// inclusive) using pivot element p that is randomply picked from a, such that the
// resulting slice has the form {...<=p..., p, ...>p...}.
// Returns the index of the pivot element.
int partition(int* a, int low, int high) {
    require_not_null(a);
    require("valid bounds", 0 <= low && low <= high);
    if (low == high) return low;
    assert("", low < high);
    int pi = low + i_rnd(high - low + 1);
    int p = a[pi];
    a[pi] = a[low];
    a[low] = p;
    int i = low + 1, j = high;
    assert("", i <= j);
    while (i <= j) {
        assert("lower part <= p", forall_x(int k = low, k < i, k++, a[k] <= p));
        assert("upper part > p", forall_x(int k = j + 1, k <= high, k++, a[k] > p));
        while (i <= j && a[i] <= p) i++;
        assert("", i > j || a[i] > p); 
        if (i > j) break;
        assert("", a[i] > p); 
        while (i <= j && a[j] > p) j--;
        assert("", a[i] > p && (i > j || a[j] <= p));
        if (i > j) break;
        assert("", i < j && a[i] > p && a[j] <= p);
        int h = a[i];
        a[i] = a[j];
        a[j] = h;
        assert("", i < j && a[i] <= p && a[j] > p);
        i++; j--;
    }
    assert("", i == j + 1);
    assert("lower part <= p", forall_x(int k = low, k <= j, k++, a[k] <= p));
    assert("upper part > p", forall_x(int k = j + 1, k <= high, k++, a[k] > p));
    int h = a[j];
    a[j] = p;
    a[low] = h;
    ensure("lower part <= p", forall_x(int k = low, k <= j, k++, a[k] <= p));
    ensure("upper part > p", forall_x(int k = j + 1, k <= high, k++, a[k] > p));
    return j;
}

// Represents an interval with inclusive boundaries.
typedef struct {
    int low, high;
} Interval;

_Static_assert(sizeof(void*) == sizeof(Interval), "valid size");

// Sends an interval to the channel.
void uchan_send_interval(UChan* ch, int low, int high) {
    long int i = ((long int)high << 32) | ((long int)low & 0xffffffffL);
    uchan_send(ch, (void*)i);
}

// Receives an interval from the channel. If necessary, blocks until an interval
// is available. Returns false if the channel has been closed and no more intervals
// are available.
bool uchan_receive_interval(UChan* ch, Interval* i) {
    return uchan_receive2(ch, (void*)i);
}

// Recursively computes the n-th Fibonacci number.
int fib(int n) {
    if (n <= 1) {
        return 1;
    } else {
        return fib(n - 1) + fib(n - 2);
    }
}

// Represents the set of arguments given to the thread function.
typedef struct Args Args;
struct Args {
    int* arr;
    UChan* ch_work; // work channel, contains intervals
    UChan* ch_results; // dummy results channel
    Countdown* c;
};

// The worker thread function repeatedly picks an interval from the channel,
// partitions the corresponding array slice, and (if the slices left or right of
// the pivot element have at least length two) writes new intervals to the channel.
// The worker thread finishes when the channel has been closed and no more
// intervals are available.
void* thread_func(void* arg) {
    // stderr_log("stacksize = %lu", get_stacksize());
    require_not_null(arg);
    Args* a = arg;
    // counter for the number of elements that this thread partitioned,
    // represents the amount of work that this thread performed
    int partitioned_elements = 0;
    // number of elements that this thread sorted to its final position
    int sorted_elements = 0;
    Interval i;
    while (uchan_receive_interval(a->ch_work, &i)) {
        //stderr_log("low = %d, high = %d", i.low, i.high);
        assert("bounds not negative and interval has at least two elements", 0 <= i.low && i.low < i.high);

#ifdef ENABLE_FIB
        // do some more artificial work on the thread's stack
        for (int i = 0; i < 200; i++) {
            int x = fib(20); // work channel, contains intervals
            uchan_send_int(a->ch_results, x); // dummy results channel
        }
        // pthread_yield_np();
#endif

        int p = partition(a->arr, i.low, i.high);
        partitioned_elements += i.high - i.low + 1;
        sorted_elements++;
        countdown_dec(a->c);
        int n_left = (p - 1) - i.low + 1;
        if (n_left > 1) {
            uchan_send_interval(a->ch_work, i.low, p - 1);
        } else if (n_left == 1) {
            sorted_elements++;
            countdown_dec(a->c);
        }
        int n_right = i.high - (p + 1) + 1;
        if (n_right > 1) {
            uchan_send_interval(a->ch_work, p + 1, i.high);
        } else if (n_right == 1) {
            sorted_elements++;
            countdown_dec(a->c);
        }
    }
    stderr_log("partitioned_elements = %d, sorted_elements = %d",
                partitioned_elements, sorted_elements);
    assert("countdown finished", countdown_finished(a->c));
    return NULL;
}

int main(void) {
    stderr_log("stacksize = %lu", get_stacksize());

    int n_arr = ARR_LENGTH;
    UChan* ch_work = uchan_new(); // work channel, contains intervals
    UChan* ch_results = uchan_new(); // dummy results channel
    Countdown* countdown = countdown_new(n_arr); // to determine when we are done
    int error;

    // fill the array with random numbers
    int* arr = xmalloc(n_arr * sizeof(int));
    for (int i = 0; i < n_arr; i++) {
        arr[i] = i_rnd(10 * n_arr);
    }

    timespec start = time_now();

    // start the threads
    int n_threads = N_THREADS;
    pthread_t threads[n_threads];
    for (int i = 0; i < n_threads; i++) {
        error = pthread_create(&threads[i], NULL, thread_func,
                               &(Args){arr, ch_work, ch_results, countdown});
        panic_if(error != 0, "error %d", error);
    }

    // the initial interval is the whole array
    uchan_send_interval(ch_work, 0, n_arr - 1);

    // wait for countdown to reach zero
    countdown_wait(countdown);

    // close the channel and wait for the threads to finish
    uchan_close(ch_work);
    for (int i = 0; i < n_threads; i++) {
        error = pthread_join(threads[i], NULL);
        panic_if(error != 0, "error %d", error);
    }
    countdown_free(countdown);
    uchan_free(ch_work);
    uchan_free(ch_results);

    printf("time = %.1f ms\n", time_ms_since(start));
    ensure("sorted", forall(i, n_arr - 1, arr[i] <= arr[i+1]));
    free(arr);

    return 0;
}
