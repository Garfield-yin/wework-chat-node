#ifndef WEWORKCHAT_H
#define WEWORKCHAT_H

#include <napi.h>
#include <thread>

#include "WeWorkFinanceSdk_C.h"


struct TsfnContext {
  TsfnContext(Napi::Env env) : deferred(Napi::Promise::Deferred::New(env)) {
    
  };
  // Native Promise returned to JavaScript
  Napi::Promise::Deferred deferred;

  // Native thread
  std::thread nativeThread;

  Napi::ThreadSafeFunction tsfn;
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

    
 private:
    
    Napi::Value EndFetchData(const Napi::CallbackInfo& info);
    Napi::Value StartFetchData(const Napi::CallbackInfo& info);
    Napi::Value GetMediaData(const Napi::CallbackInfo& info);
    Napi::Value GetChat(const Napi::CallbackInfo& info);
        
    int64_t parseJsonData(TsfnContext *context,const char *data);
    static void* fetchData(TsfnContext *context,void *arg);
  
    int initSdk(const Napi::CallbackInfo& info);
    
    
    std::string corpid_;
    std::string secret_;
    WeWorkFinanceSdk_t *sdk_;
    std::mutex mtx_;
    std::string private_key_;
    int64_t seq_;
    bool end_;
};


std::string decode64(const std::string &ascdata);
std::string rsa_pri_decrypt(const std::string &cipherText, const char *priKey);
#endif


