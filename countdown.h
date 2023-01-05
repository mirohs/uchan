/*
@author: Michael Rohs
@date: January 5, 2023
*/

#ifndef countdown_h_INCLUDED
#define countdown_h_INCLUDED

#include "util.h"

typedef struct Countdown Countdown;

Countdown* countdown_new(int n);
void countdown_add(Countdown* c, int i);
void countdown_sub(Countdown* c, int i);
void countdown_inc(Countdown* c);
void countdown_dec(Countdown* c);
void countdown_wait(Countdown* c);
void countdown_free(Countdown* c);
void countdown_set(Countdown* c, int i);
int countdown_get(Countdown* c);
bool countdown_finished(Countdown* c);

#endif // countdown_h_INCLUDED
