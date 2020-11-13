#ifndef __RING_H__
#define __RING_H__

#define PATH_LEN_MAX (0x100)

struct ring {
    char self[PATH_LEN_MAX];
    char next[PATH_LEN_MAX];
    char prev[PATH_LEN_MAX];
};

void ring_init(const char *in);
void ring_set_next(const char *in);
void ring_set_prev(const char *in);

#endif
