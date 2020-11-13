#ifndef __RPC_H__
#define __RPC_H__

#include <stdint.h>
#include <margo.h>
#include <mercury_proc_string.h>

typedef struct {
    int32_t n;
    hg_string_t *host;
} list_t;

void init_rpc(margo_instance_id mid, hg_addr_t addr);

MERCURY_GEN_PROC(join_in_t, ((hg_const_string_t)(address)))

MERCURY_GEN_PROC(join_out_t,
                 ((hg_const_string_t)(next))\
                 ((hg_const_string_t)(prev)))

MERCURY_GEN_PROC(set_next_in_t, ((hg_const_string_t)(address)))
MERCURY_GEN_PROC(set_prev_in_t, ((hg_const_string_t)(address)))

hg_return_t hg_proc_list_t(hg_proc_t proc, void *data);

#endif
