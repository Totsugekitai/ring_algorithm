#ifndef __MAIN_H__
#define __MAIN_H__

#include <stdbool.h>
#include <time.h>
#include <margo.h>
#include <mercury_proc_string.h>

#define TIMEOUT_MSEC (3000)

struct env {
    margo_instance_id mid;
    hg_addr_t this, next, prev;
    hg_id_t join_rpc, set_next_rpc, set_prev_rpc, list_rpc, coordinator_rpc, election_rpc;
    bool is_coordinator, done_election, received_heartbeat;
    time_t prev_heartbeat;
};

#endif
