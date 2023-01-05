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
bool uchan_receive2_noblock(UChan* ch, /*in*/void** x);

#if 0
typedef struct UChanSelect UChanSelect;
UChanSelect* uchan_select_new(void);
void uchan_select_free(UChanSelect* cs);
UChan* uchan_select(UChanSelect* cs);
UChan* uchan_select_noblock(UChanSelect* cs);
void uchan_select_receive(UChanSelect* cs, UChan* ch, void** x, /*out*/bool* has_value);
void uchan_select_receive_int(UChanSelect* cs, UChan* ch, int* x, /*out*/bool* has_value);
#endif

#endif // uchan_h_INCLUDED
