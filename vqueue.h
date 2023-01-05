/*
@author: Michael Rohs
@date: January 5, 2023
*/

#ifndef vqueue_h_INCLUDED
#define vqueue_h_INCLUDED

#include "util.h"

typedef struct VQueue VQueue;

VQueue* vqueue_new(void);
void vqueue_free(VQueue* q);
void vqueue_put(VQueue* q, void* x);
void* vqueue_get(VQueue* q);
bool vqueue_empty(VQueue* q);
int vqueue_len(VQueue* q);

#endif // vqueue_h_INCLUDED
