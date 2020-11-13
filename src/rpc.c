#include "rpc.h"
#include "ring.h"
#include "main.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#include <margo.h>
#include <mercury_proc_string.h>

extern struct env env;
extern struct ring ring;

void join(hg_handle_t h);
DECLARE_MARGO_RPC_HANDLER(join)
void set_next(hg_handle_t h);
DECLARE_MARGO_RPC_HANDLER(set_next)
void set_prev(hg_handle_t h);
DECLARE_MARGO_RPC_HANDLER(set_prev)
void list(hg_handle_t h);
DECLARE_MARGO_RPC_HANDLER(list)
void coordinator(hg_handle_t h);
DECLARE_MARGO_RPC_HANDLER(coordinator)
void election(hg_handle_t h);
DECLARE_MARGO_RPC_HANDLER(election)

void init_rpc(margo_instance_id mid, hg_addr_t addr)
{
    env.mid = mid;
    env.join_rpc = MARGO_REGISTER(mid, "join", join_in_t, join_out_t, join);
    env.set_next_rpc = MARGO_REGISTER(mid, "set_next", hg_string_t, void, set_next);
    margo_registered_disable_response(mid, env.set_next_rpc, HG_TRUE);
    env.set_prev_rpc = MARGO_REGISTER(mid, "set_prev", hg_string_t, void, set_prev);
    margo_registered_disable_response(mid, env.set_prev_rpc, HG_TRUE);
    env.list_rpc = MARGO_REGISTER(mid, "list", list_t, void, list);
    margo_registered_disable_response(mid, env.list_rpc, HG_TRUE);
    env.coordinator_rpc = MARGO_REGISTER(mid, "coordinator", list_t, void, coordinator);
    margo_registered_disable_response(mid, env.coordinator_rpc, HG_TRUE);
    env.election_rpc = MARGO_REGISTER(mid, "election", list_t, void, election);
    margo_registered_disable_response(mid, env.election_rpc, HG_TRUE);

    env.this = addr;
}

void join(hg_handle_t h)
{
    hg_return_t ret;
    join_in_t in;
    join_out_t out;

    // RPC呼び出し元からの入力受け取り
    ret = margo_get_input(h, &in);
    assert(ret == HG_SUCCESS);

    // 自らのサーバアドレスの取得
    hg_addr_t my_address;
    margo_addr_self(env.mid, &my_address);
    char addr_str[PATH_LEN_MAX];
    size_t addr_str_size = sizeof(addr_str);
    margo_addr_to_string(env.mid, addr_str, &addr_str_size, my_address);
    margo_addr_free(env.mid, my_address);

    // set_next RPC
    hg_handle_t sh;
    // ターゲットのサーバのアドレスをルックアップ
    hg_addr_t prev_addr;
    ret = margo_addr_lookup(env.mid, ring.prev, &prev_addr);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "margo address lookup error\n");
        exit(1);
    }
    // set_next RPCのハンドルを作成
    ret = margo_create(env.mid, prev_addr, env.set_next_rpc, &sh);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "margo handle create error\n");
        exit(1);
    }
    // set_next RPC呼び出し
    set_next_in_t nin;
    nin.address = in.address;
    ret = margo_forward_timed(sh, &nin, TIMEOUT_MSEC);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "margo forward rpc error\n");
        exit(1);
    }
    char old_prev[PATH_LEN_MAX];
    strcpy(old_prev, ring.prev);
    ring_set_prev(in.address);
    // 後始末
    ret = margo_destroy(sh);
    assert(ret ==HG_SUCCESS);

    // RPC呼び出し元へのレスポンス
    out.next = ring.next;
    out.prev = old_prev;
    ret = margo_respond(h, &out);
    assert(ret == HG_SUCCESS);

    // 後処理
    ret = margo_free_input(h, &in);
    assert(ret == HG_SUCCESS);
    ret = margo_destroy(h);
    assert(ret ==HG_SUCCESS);
}
DEFINE_MARGO_RPC_HANDLER(join)

void set_next(hg_handle_t h)
{
     hg_return_t ret;
     set_next_in_t in;

     ret = margo_get_input(h, &in);
     assert(ret == HG_SUCCESS);

     ring_set_next(in.address);

     ret = margo_free_input(h, &in);
     assert(ret == HG_SUCCESS);

     ret = margo_destroy(h);
     assert(ret == HG_SUCCESS);
}
DEFINE_MARGO_RPC_HANDLER(set_next)

void set_prev(hg_handle_t h)
{
    hg_return_t ret;
    set_prev_in_t in;

    ret = margo_get_input(h, &in);
    assert(ret == HG_SUCCESS);

    ring_set_prev(in.address);

    ret = margo_free_input(h, &in);
    assert(ret == HG_SUCCESS);

    ret = margo_destroy(h);
    assert(ret == HG_SUCCESS);
}
DEFINE_MARGO_RPC_HANDLER(set_prev)

void list(hg_handle_t h)
{
    printf("list RPC\n");
    env.prev_heartbeat = time(NULL);
    hg_return_t ret;
    list_t in;

    // RPC入力を受け取る
    ret = margo_get_input(h, &in);
    assert(ret == HG_SUCCESS);

    bool have_own_addr = false;
    for (int i = 0; i < in.n; i++) {
        if (strcmp(ring.self, in.host[i]) == 0) {
            have_own_addr = true;
            break;
        }
    }
    if (have_own_addr) {
        for (int i = 0; i < in.n; i++) {
            printf("list address %d (%s)\n", i, in.host[i]);
        }
        return;
    }

    // list_tを増やす
    //printf("list rpc debug print\n");
    list_t list;
    list.host = malloc(sizeof(hg_string_t) * (in.n + 1));
    assert(list.host);
    int i;
    for (i = 0; i < in.n; i++) {
        list.host[i] = in.host[i];
    }
    list.host[i] = ring.self;
    list.n = in.n + 1;

    hg_handle_t next_h;
    // ターゲットのサーバのアドレスをルックアップ
    hg_addr_t next_addr;
    ret = margo_addr_lookup(env.mid, ring.next, &next_addr);
    assert(ret == HG_SUCCESS);
    // set_next RPCのハンドルを作成
    ret = margo_create(env.mid, next_addr, env.list_rpc, &next_h);
    assert(ret == HG_SUCCESS);
    // list RPCをnextに送信
    ret = margo_forward_timed(next_h, &list, TIMEOUT_MSEC);
    assert(ret == HG_SUCCESS);
    ret = margo_destroy(next_h);
    assert(ret == HG_SUCCESS);
    // 入力をfree
    ret = margo_free_input(h, &in);
    assert(ret == HG_SUCCESS);

    ret = margo_destroy(h);
    assert(ret == HG_SUCCESS);
}
DEFINE_MARGO_RPC_HANDLER(list)

// list_tのシリアライザ
hg_return_t hg_proc_list_t(hg_proc_t proc, void *data)
{
    list_t *l = data;
    hg_return_t ret;

    ret = hg_proc_int32_t(proc, &l->n);
    assert(ret == HG_SUCCESS);

    if (hg_proc_get_op(proc) == HG_DECODE) {
        l->host = malloc(sizeof(hg_string_t) * l->n);
    }
    assert(l->host);
    for (int i = 0; i < l->n; i++) {
        ret = hg_proc_hg_string_t(proc, &l->host[i]);
        assert(ret == HG_SUCCESS);
    }
    if (hg_proc_get_op(proc) == HG_FREE) {
        free(l->host);
    }
    return ret;
}

void coordinator(hg_handle_t h)
{
    printf("coordinator RPC\n");
    if (env.is_coordinator) {
        return;
    }
    hg_addr_t next_addr;
    hg_handle_t next_h;
    hg_return_t ret;
    list_t in;

    // RPC入力を受け取る
    ret = margo_get_input(h, &in);
    assert(ret == HG_SUCCESS);

    // 最大値を持つホストを判定
    hg_addr_t max_host = 0;
    int max_index = 0;
    for (int i = 0; i < in.n; i++) {
        hg_addr_t host_addr;
        ret = margo_addr_lookup(env.mid, in.host[i], &host_addr);
        if (max_host < host_addr) {
            max_host = host_addr;
            max_index = i;
        }
    }
    // もし最大のホストが自分だったらフラグを立てる
    if (strcmp(ring.self, in.host[max_index]) == 0) {
        env.is_coordinator = true;
        return;
    } else {
        env.is_coordinator = false;
    }
    list_t list;
    list.n = in.n;
    list.host = malloc(sizeof(hg_string_t) * in.n);
    assert(list.host);
    for (int i = 0; i < in.n; i++) {
        list.host[i] = in.host[i];
    }

    // ターゲットのサーバのアドレスをルックアップ
    ret = margo_addr_lookup(env.mid, ring.next, &next_addr);
    assert(ret == HG_SUCCESS);
    // coordinator RPCのハンドルを作成
    ret = margo_create(env.mid, next_addr, env.coordinator_rpc, &next_h);
    assert(ret == HG_SUCCESS);
    // coordinator RPCをnextに送信
    ret = margo_forward_timed(next_h, &list, TIMEOUT_MSEC);
    assert(ret == HG_SUCCESS);
    ret = margo_destroy(next_h);
    assert(ret == HG_SUCCESS);
    // 入力をfree
    ret = margo_free_input(h, &in);
    assert(ret == HG_SUCCESS);

    ret = margo_destroy(h);
    assert(ret == HG_SUCCESS);
}
DEFINE_MARGO_RPC_HANDLER(coordinator)

void election(hg_handle_t h)
{
    printf("election RPC\n");
    hg_return_t ret;
    list_t in;
    hg_handle_t next_h;
    hg_addr_t next_addr;

    // RPC入力を受け取る
    ret = margo_get_input(h, &in);
    assert(ret == HG_SUCCESS);

    bool have_own_addr = false;
    for (int i = 0; i < in.n; i++) {
        if (strcmp(ring.self, in.host[i]) == 0) {
            have_own_addr = true;
            break;
        }
    }
    if (have_own_addr) {
        goto after_get_list;
    }

    // list_tを増やす
    list_t list;
    list.host = malloc(sizeof(hg_string_t) * (in.n + 1));
    assert(list.host);
    int i;
    for (i = 0; i < in.n; i++) {
        list.host[i] = in.host[i];
    }
    list.host[i] = ring.self;
    list.n = in.n + 1;

    // ターゲットのサーバのアドレスをルックアップ
    ret = margo_addr_lookup(env.mid, ring.next, &next_addr);
    assert(ret == HG_SUCCESS);
    // list RPCのハンドルを作成
    ret = margo_create(env.mid, next_addr, env.list_rpc, &next_h);
    assert(ret == HG_SUCCESS);
    // list RPCをnextに送信
    ret = margo_forward_timed(next_h, &list, TIMEOUT_MSEC);
    assert(ret == HG_SUCCESS);
    ret = margo_destroy(next_h);
    assert(ret == HG_SUCCESS);
    // 入力をfree
    ret = margo_free_input(h, &in);
    assert(ret == HG_SUCCESS);

    ret = margo_destroy(h);
    assert(ret == HG_SUCCESS);

    return;

    list_t host_list;
after_get_list:
    if (env.done_election) {
        return;
    }
    env.done_election = true;

    host_list.n = in.n;
    host_list.host = malloc(sizeof(hg_string_t) * in.n);
    assert(host_list.host);
    for (int k = 0; k < in.n; k++) {
        host_list.host[k] = in.host[k];
    }
    // ターゲットのサーバのアドレスをルックアップ
    ret = margo_addr_lookup(env.mid, ring.next, &next_addr);
    assert(ret == HG_SUCCESS);
    // coordinator RPCのハンドルを作成
    ret = margo_create(env.mid, next_addr, env.coordinator_rpc, &next_h);
    assert(ret == HG_SUCCESS);
    // coordinator RPCをnextに送信
    ret = margo_forward_timed(next_h, &host_list, TIMEOUT_MSEC);
    assert(ret == HG_SUCCESS);
    ret = margo_destroy(next_h);
    assert(ret == HG_SUCCESS);

    // 入力をfree
    ret = margo_free_input(h, &in);
    assert(ret == HG_SUCCESS);

    ret = margo_destroy(h);
    assert(ret == HG_SUCCESS);
}
DEFINE_MARGO_RPC_HANDLER(election)
