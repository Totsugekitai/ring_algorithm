#include "ring.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern struct ring ring;

void ring_init(const char *in)
{
    memset(ring.self, 0, sizeof(ring.self));
    memset(ring.next, 0, sizeof(ring.next));
    memset(ring.prev, 0, sizeof(ring.prev));
    strcpy(ring.self, in);
    strcpy(ring.next, in);
    strcpy(ring.prev, in);
    printf("self: %s\n", ring.self);
    printf("next: %s\n", ring.next);
    printf("prev: %s\n", ring.prev);
}

void ring_set_next(const char *in)
{
    memset(ring.next, 0, sizeof(ring.next));
    strcpy(ring.next, in);
    printf("==================================================\n");
    printf("next: %s\n", ring.next);
    printf("prev: %s\n", ring.prev);
}

void ring_set_prev(const char *in)
{
    memset(ring.prev, 0, sizeof(ring.prev));
    strcpy(ring.prev, in);
    printf("==================================================\n");
    printf("next: %s\n", ring.next);
    printf("prev: %s\n", ring.prev);
}

