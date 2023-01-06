/*
@author: Michael Rohs
@date: January 5, 2023
*/

#ifndef uchan_h_INCLUDED
#define uchan_h_INCLUDED

#include "util.h"

typedef struct UChan UChan;

UChan* uchan_new(void);
void uchan_free(UChan* ch);
int uchan_len(UChan* ch);
void uchan_close(UChan* ch);

void uchan_send(UChan* ch, void* x);
void* uchan_receive(UChan* ch);
void uchan_send_int(UChan* ch, int x);
int uchan_receive_int(UChan* ch);

bool uchan_receive2(UChan* ch, void** x);
bool uchan_receive2_int(UChan* ch, int* x);
bool uchan_receive2_noblock(UChan* ch, void** x);

int uchan_select(UChan** channels, int n_channels, void** x, bool* has_value);

#endif // uchan_h_INCLUDED
