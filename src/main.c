#include "main.h"
#include "ring.h"
#include "rpc.h"

#include <unistd.h>
#include <assert.h>
#include <margo.h>
#include <mercury_proc_string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>

struct env env;
struct ring ring;

static void join_ring(const char *target_addr_str);
static void list_start();
static void election_start();
bool check_heartbeat(time_t t);
void *handle_sig(void *arg);

int main(int argc, char *argv[])
{
    pthread_t t;
    static sigset_t sigset;

    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    sigaddset(&sigset, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &sigset, NULL);
    pthread_create(&t, NULL, handle_sig, &sigset);
    pthread_detach(t);

    // サーバの初期化
    margo_instance_id mid = margo_init("tcp", MARGO_SERVER_MODE, 1, 10);
    assert(mid);

    // サーバアドレスの取得
    hg_addr_t my_address;
    margo_addr_self(mid, &my_address);
    char addr_str[PATH_LEN_MAX];
    size_t addr_str_size = sizeof(addr_str);
    margo_addr_to_string(mid, addr_str, &addr_str_size, my_address);
    //margo_addr_free(mid, my_address);

    printf("Server running at address %s\n", addr_str);

    // RPCの登録
    init_rpc(mid, my_address);

    // ring構造体の初期化
    ring_init(addr_str);

    // アドレスが引数で指定されていたらそのアドレスのサーバのnextになる形で参加する
    if (argc > 1) {
        env.is_coordinator = false;
        join_ring(argv[1]);
    } else {
        env.is_coordinator = true;
    }

    while (true) {
        if (env.is_coordinator) {
            list_start(ring.self);
        } else {
            time_t t = time(NULL);
            bool heartbeat = check_heartbeat(t);
            if (!heartbeat) {
                election_start(ring.self);
                sleep(1);
            }
        }
        env.done_election = false;
        sleep(1);
    }

    // メインループ
    margo_wait_for_finalize(mid);

    return 0;
}

bool check_heartbeat(time_t t)
{
    // 前回のハートビートから3秒以上経ってたらfalseを返す
    if (env.prev_heartbeat + 3 <= t) {
        return false;
    } else {
        return true;
    }
}

void print_server_address(hg_addr_t address, char *buf, size_t size)
{
    margo_addr_to_string(env.mid, buf, &size, address);
    printf("%s\n", buf);
}

static void join_ring(const char *target_addr_str)
{
    hg_handle_t h;
    hg_return_t ret;
    join_in_t in;
    join_out_t out;

    // ターゲットのサーバのアドレスをルックアップ
    hg_addr_t target_addr;
    ret = margo_addr_lookup(env.mid, target_addr_str, &target_addr);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "margo address lookup error\n");
        exit(1);
    }
    // ターゲットのサーバのアドレスをnextにしまう
    strcpy(ring.next, target_addr_str);

    // join RPCのハンドルを作成
    ret = margo_create(env.mid, target_addr, env.join_rpc, &h);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "margo handle create error\n");
        exit(1);
    }

    // サーバアドレスの取得
    hg_addr_t my_address;
    margo_addr_self(env.mid, &my_address);
    char addr_str[PATH_LEN_MAX];
    size_t addr_str_size = sizeof(addr_str);
    margo_addr_to_string(env.mid, addr_str, &addr_str_size, my_address);
    margo_addr_free(env.mid, my_address);

    // join RPC呼び出し(タイムアウト機能付き)
    in.address = addr_str;
    ret = margo_forward_timed(h, &in, TIMEOUT_MSEC);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "margo forward rpc error\n");
        exit(1);
    }

    // RPC先からの出力受け取り
    ret = margo_get_output(h, &out);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "margo get rpc output error\n");
        exit(1);
    }
    printf("next: %s\n", out.next);
    printf("prev: %s\n", out.prev);

    ring_set_prev(out.prev);

    // 解放
    ret = margo_free_output(h, &out);

    // 削除
    ret = margo_destroy(h);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "margo destroy handler error\n");
        exit(1);
    }
}

static void election_start(char *self)
{
    //printf("election_start start\n");

    env.done_election = false;

    list_t list;
    list.n = 0;
    list.host = &self;
    hg_handle_t next_h;
    hg_return_t ret;
    // ターゲットのサーバのアドレスをルックアップ
    hg_addr_t next_addr;
    ret = margo_addr_lookup(env.mid, ring.next, &next_addr);
    assert(ret == HG_SUCCESS);
    //printf("lookup target server address\n");
    // election RPCのハンドルを作成
    ret = margo_create(env.mid, next_addr, env.election_rpc, &next_h);
    assert(ret == HG_SUCCESS);
    //printf("election rpc create\n");
    // election RPCをnextに送信
    ret = margo_forward_timed(next_h, &list, TIMEOUT_MSEC);
    assert(ret == HG_SUCCESS);
    //printf("election rpc send\n");
    ret = margo_destroy(next_h);
    assert(ret == HG_SUCCESS);
}

static void list_start(char *self) {
    //printf("list_start start\n");
    list_t list;
    list.n = 0;
    list.host = &self;
    hg_handle_t next_h;
    hg_return_t ret;
    // ターゲットのサーバのアドレスをルックアップ
    hg_addr_t next_addr;
    ret = margo_addr_lookup(env.mid, ring.next, &next_addr);
    assert(ret == HG_SUCCESS);
    //printf("lookup target server address\n");
    // list RPCのハンドルを作成
    ret = margo_create(env.mid, next_addr, env.list_rpc, &next_h);
    assert(ret == HG_SUCCESS);
    //printf("list rpc create\n");
    // list RPCをnextに送信
    ret = margo_forward_timed(next_h, &list, TIMEOUT_MSEC);
    assert(ret == HG_SUCCESS);
    //printf("list rpc send\n");
    ret = margo_destroy(next_h);
    assert(ret == HG_SUCCESS);
}

static void leave() {
    hg_handle_t h;
    hg_return_t ret;
    /* prevにset_next RPCを発行してprevのnextにnextを代入する */
    // ターゲットのサーバのアドレスをルックアップ
    hg_addr_t prev_addr;
    ret = margo_addr_lookup(env.mid, ring.prev, &prev_addr);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "margo address lookup error\n");
        exit(1);
    }
    // set_next RPCのハンドルを作成
    ret = margo_create(env.mid, prev_addr, env.set_next_rpc, &h);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "margo handle create error\n");
        exit(1);
    }
    // set_next RPC呼び出し
    set_next_in_t nin;
    nin.address = ring.next;
    ret = margo_forward_timed(h, &nin, TIMEOUT_MSEC);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "margo forward rpc error\n");
        exit(1);
    }
    // 削除
    ret = margo_destroy(h);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "margo destroy handler error\n");
        exit(1);
    }
    /* nextにset_prev RPCを発行してnextのprevにprevを代入する */
    // ターゲットのサーバのアドレスをルックアップ
    hg_addr_t next_addr;
    ret = margo_addr_lookup(env.mid, ring.next, &next_addr);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "margo address lookup error\n");
        exit(1);
    }
    // set_prev RPCのハンドルを作成
    ret = margo_create(env.mid, next_addr, env.set_prev_rpc, &h);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "margo handle create error\n");
        exit(1);
    }
    // set_prev RPC呼び出し
    set_prev_in_t pin;
    pin.address = ring.prev;
    ret = margo_forward_timed(h, &pin, TIMEOUT_MSEC);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "margo forward rpc error\n");
        exit(1);
    }
    // 削除
    ret = margo_destroy(h);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "margo destroy handler error\n");
        exit(1);
    }
}

void *handle_sig(void *arg)
{
    sigset_t *a = arg;
    int sig;

    sigwait(a, &sig);
    leave();
    exit(1);
}
