#ifndef WEWORKCHAT_H
#define WEWORKCHAT_H

#include <napi.h>
#include <atomic>
#include <string>
#include <thread>

#include "WeWorkFinanceSdk_C.h"

class WeWorkChat;

struct TsfnContext {
  TsfnContext(Napi::Env env) : deferred(Napi::Promise::Deferred::New(env)) {

  };
  // Native Promise returned to JavaScript
  Napi::Promise::Deferred deferred;

  // Native thread
  std::thread nativeThread;

  Napi::ThreadSafeFunction tsfn;

  // 发起 fetch 的实例，Finalizer 里用它解除对 JS 对象的强引用
  WeWorkChat *owner = nullptr;
};

struct MsgData {
    char *msg_data;
    Slice_t *slice_msg;
};

//public Napi::AsyncWorker
class WeWorkChat : public Napi::ObjectWrap<WeWorkChat>{
 public:
  static Napi::Object Init(Napi::Env env, Napi::Object exports);
    WeWorkChat(const Napi::CallbackInfo& info);
    ~WeWorkChat();

    // fetch 线程退出、tsfn 销毁后由 FinalizerCallback 调用
    void ReleaseFetchRef();

 private:

    Napi::Value EndFetchData(const Napi::CallbackInfo& info);
    Napi::Value StartFetchData(const Napi::CallbackInfo& info);
    Napi::Value GetMediaData(const Napi::CallbackInfo& info);
    Napi::Value GetChat(const Napi::CallbackInfo& info);

    int64_t parseJsonData(TsfnContext *context,const char *data);
    static void* fetchData(TsfnContext *context,void *arg);

    int initSdk(const Napi::CallbackInfo& info);
    // sdk 可用性检查：init 失败或 stopFetch 释放之后再调用底层接口会直接崩溃
    bool ensureSdk(Napi::Env env);
    // 释放 sdk，重复调用只生效一次，避免 double free
    void destroySdk();


    std::string corpid_;
    std::string secret_;
    WeWorkFinanceSdk_t *sdk_;
    std::string private_key_;
    std::atomic<int64_t> seq_;
    std::atomic<bool> end_;
    // fetch 线程是否还在运行，stopFetch 靠它判断何时可以安全释放 sdk
    std::atomic<bool> fetching_;
    std::atomic<bool> sdk_destroyed_;
};


std::string decode64(const std::string &ascdata);
std::string rsa_pri_decrypt(const std::string &cipherText, const char *priKey);
#endif
