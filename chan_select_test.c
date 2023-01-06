#include "unistd.h"
#include "uchan.h"

typedef struct {
    UChan* ch;
    int i_ch;
} Args;

Args* new_args(UChan* ch, int i_ch) {
    Args* a = xcalloc(1, sizeof(Args));
    a->ch = ch;
    a->i_ch = i_ch;
    return a;
}

void* producer_func(void* arg) {
    stderr_log("start");
    Args* a = arg;
    for (int x = 0; x < 1; x++) {
        sleep(a->i_ch == 3 ? 1 : 2);
        stderr_log("produced %d", x);
        uchan_send_int(a->ch, 10 * a->i_ch + x);
    }
    return NULL;
}

int main(void) {
    int n_chs = 3;
    UChan* chs[n_chs];
    pthread_t threads[n_chs];
    int error;

    for (int i = 0; i < n_chs; i++) {
        chs[i] = uchan_new();
        Args* a = new_args(chs[i], i);
        error = pthread_create(&threads[i], NULL, producer_func, a);
        panic_if(error != 0, "error %d", error);
    }

    //sleep(1);
    void* x;
    bool ok;
    switch (uchan_select(chs, 3, &x, &ok)) {
        case 0:
            stderr_log("channel 0: x = %d, ok = %d", (int)(long int)x, ok);
            break;
        case 1:
            stderr_log("channel 1: x = %d, ok = %d", (int)(long int)x, ok);
            break;
        case 2:
            stderr_log("channel 2: x = %d, ok = %d", (int)(long int)x, ok);
            break;
        default:
            assert("unknown channel", false);
            break;
    }
 
    for (int i = 0; i < n_chs; i++) {
        error = pthread_join(threads[i], NULL);
        panic_if(error != 0, "error %d", error);
    }
    for (int i = 0; i < n_chs; i++) {
        uchan_close(chs[i]);
        uchan_free(chs[i]);
    }

    stderr_log("main end");
    return 0;
}
