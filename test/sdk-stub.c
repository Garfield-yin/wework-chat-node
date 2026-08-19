/*
 * 测试用的桩 SDK。
 *
 * 通过 LD_PRELOAD 覆盖真实 libWeWorkFinanceSdk_C.so 里的几个符号，这样就能在
 * 没有企业微信凭据、不联网的情况下：
 *
 *   1. 记录 addon 实际传给 GetChatData 的参数（验证 max_results 等是否透传）；
 *   2. 统计 NewSlice / FreeSlice 是否配对（验证 Slice_t 有没有泄漏）；
 *   3. 让服务端返回各种异常响应（验证错误分支不崩溃、不泄漏）。
 *
 * NewSdk / Init / DestroySdk 不覆盖，走真实实现即可。
 *
 * 环境变量：
 *   STUB_MODE    ok | ret_err | retry_err | bad_json | errcode | no_chatdata
 *   STUB_REPORT  报告输出路径（JSON）
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Slice_t { char *buf; int len; } Slice_t;
typedef struct WeWorkFinanceSdk_t WeWorkFinanceSdk_t;

#define MAX_CALLS 256

static int alloc_count = 0;
static int free_count = 0;
static int call_count = 0;
static struct { unsigned long long seq; unsigned int limit; int timeout; } calls[MAX_CALLS];

Slice_t *NewSlice(void) {
    __sync_fetch_and_add(&alloc_count, 1);
    return (Slice_t *)calloc(1, sizeof(Slice_t));
}

void FreeSlice(Slice_t *s) {
    if (s == NULL) return;
    __sync_fetch_and_add(&free_count, 1);
    free(s->buf);
    free(s);
}

char *GetContentFromSlice(Slice_t *s) { return s ? s->buf : NULL; }
int GetSliceLen(Slice_t *s) { return s ? s->len : 0; }

static void set_slice(Slice_t *s, const char *json) {
    if (s == NULL) return;
    free(s->buf);
    s->buf = strdup(json);
    s->len = (int)strlen(json);
}

int GetChatData(WeWorkFinanceSdk_t *sdk, unsigned long long seq, unsigned int limit,
                const char *proxy, const char *passwd, int timeout, Slice_t *chatDatas) {
    if (call_count < MAX_CALLS) {
        calls[call_count].seq = seq;
        calls[call_count].limit = limit;
        calls[call_count].timeout = timeout;
    }
    call_count++;

    const char *mode = getenv("STUB_MODE");
    if (mode == NULL) mode = "ok";

    /* 10000 不在 10001~10003 重试区间内，只调用一次 */
    if (strcmp(mode, "ret_err") == 0) return 10000;
    /* 10001 在重试区间内，会触发 addon 的重试逻辑 */
    if (strcmp(mode, "retry_err") == 0) return 10001;
    if (strcmp(mode, "bad_json") == 0) { set_slice(chatDatas, "{not json"); return 0; }
    if (strcmp(mode, "errcode") == 0) {
        set_slice(chatDatas, "{\"errcode\":10001,\"errmsg\":\"stub error\"}");
        return 0;
    }
    /* errcode 为 0 但没有 chatdata 字段 —— 拉到最新时的真实响应 */
    if (strcmp(mode, "no_chatdata") == 0) {
        set_slice(chatDatas, "{\"errcode\":0,\"errmsg\":\"ok\"}");
        return 0;
    }
    set_slice(chatDatas, "{\"errcode\":0,\"errmsg\":\"ok\",\"chatdata\":[]}");
    return 0;
}

__attribute__((destructor))
static void write_report(void) {
    const char *path = getenv("STUB_REPORT");
    if (path == NULL) return;
    FILE *f = fopen(path, "w");
    if (f == NULL) return;
    fprintf(f, "{\"alloc\":%d,\"free\":%d,\"leaked\":%d,\"callCount\":%d,\"calls\":[",
            alloc_count, free_count, alloc_count - free_count, call_count);
    int n = call_count < MAX_CALLS ? call_count : MAX_CALLS;
    for (int i = 0; i < n; i++) {
        fprintf(f, "%s{\"seq\":%llu,\"limit\":%u,\"timeout\":%d}",
                i ? "," : "", calls[i].seq, calls[i].limit, calls[i].timeout);
    }
    fprintf(f, "]}\n");
    fclose(f);
}
