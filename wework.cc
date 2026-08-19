#include <iostream>
#include <fstream>
#include <string>
#include <cassert>

#include <limits>
#include <thread>
#include <chrono>

#include <cmath>
#include <stdexcept>
#include <cctype>
#include "openssl/md5.h"
#include "openssl/sha.h"
#include "openssl/des.h"
#include "openssl/rsa.h"
#include "openssl/pem.h"

#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/bn.h>
#include <openssl/pem.h>

#include "include/wework/wework.h"

#include "include/rapidjson/document.h"
#include "include/rapidjson/stringbuffer.h"
#include "include/rapidjson/writer.h"
#include "include/rapidjson/error/en.h"



using namespace std;
using namespace rapidjson;

std::string ERROR_PREFIX = "WEWORK_CHAT_NODE::";

//// thread/////
void FinalizerCallback(Napi::Env env, void *finalizeData, TsfnContext *context);
// The thread-safe function finalizer callback. This callback executes
// at destruction of thread-safe function, taking as arguments the finalizer
// data and threadsafe-function context.
void FinalizerCallback(Napi::Env env, void *finalizeData,
                       TsfnContext *context) {
  // Join the thread
  context->nativeThread.join();

  // fetch 期间对 JS 对象加了强引用，线程结束后必须解除，否则实例永远不会被回收
  if (context->owner != nullptr) {
    context->owner->ReleaseFetchRef();
  }

  // Resolve the Promise previously returned to JS via the CreateTSFN method.
  context->deferred.Resolve(Napi::Boolean::New(env, true));
  delete context;
}

void MediaDataFinalizerCallback(Napi::Env env,void *finalizeData,::MediaData_t* finalizeHint);
void MediaDataFinalizerCallback(Napi::Env env,void *finalizeData,::MediaData_t* media) {
    // 释放数据
   ::FreeMediaData(media);
}

static const char reverse_table[128] = {
   64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
   64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
   64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
   52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
   64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
   15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
   64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
   41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64
};

// 企业微信单次拉取上限，超过 1000 服务端会直接返回错误
const int kMaxResults = 1000;
// getChatData 未传 timeout 时的默认超时（秒）
const int kDefaultTimeout = 30;
// fetchData 后台轮询的超时（秒）
const int kFetchTimeout = 30;
// stopFetch 等待后台线程退出的上限（毫秒）
const int kStopWaitMs = 45000;

namespace {

// NewSlice 分配的 Slice_t 必须配对 FreeSlice。之前每个 return / continue 分支
// 都要手写一次 FreeSlice，只要漏掉一个就会泄漏，改成 RAII 之后任何出口都不会漏。
class SliceGuard {
public:
    SliceGuard() : slice_(NewSlice()) {}
    ~SliceGuard() { reset(); }
    SliceGuard(const SliceGuard &) = delete;
    SliceGuard &operator=(const SliceGuard &) = delete;

    Slice_t *get() const { return slice_; }
    bool valid() const { return slice_ != nullptr; }
    // 把所有权交出去（例如交给 ThreadSafeFunction 回调负责释放）
    Slice_t *release() {
        Slice_t *s = slice_;
        slice_ = nullptr;
        return s;
    }
    void reset() {
        if (slice_ != nullptr) {
            FreeSlice(slice_);
            slice_ = nullptr;
        }
    }

private:
    Slice_t *slice_;
};

// rapidjson 在 NDEBUG（Release 构建）下 operator[] 不做成员存在性检查，
// 服务端一旦少返回一个字段就是野指针解引用，直接 Segmentation fault。
// 下面几个 helper 统一走 FindMember，缺字段时返回默认值。
const rapidjson::Value *FindMember(const rapidjson::Value &v, const char *name) {
    if (!v.IsObject()) {
        return nullptr;
    }
    rapidjson::Value::ConstMemberIterator it = v.FindMember(name);
    if (it == v.MemberEnd()) {
        return nullptr;
    }
    return &it->value;
}

const char *GetStringMember(const rapidjson::Value &v, const char *name, const char *fallback) {
    const rapidjson::Value *m = FindMember(v, name);
    if (m == nullptr || !m->IsString()) {
        return fallback;
    }
    return m->GetString();
}

int64_t GetInt64Member(const rapidjson::Value &v, const char *name, int64_t fallback) {
    const rapidjson::Value *m = FindMember(v, name);
    if (m == nullptr) {
        return fallback;
    }
    if (m->IsInt64()) {
        return m->GetInt64();
    }
    if (m->IsUint64()) {
        return static_cast<int64_t>(m->GetUint64());
    }
    return fallback;
}

// 读取数值参数：字段缺失 / 不是数字 / NaN 时都退回默认值。
// 直接 ToNumber() 拿 undefined 会得到 NaN，转成 int64_t 是未定义行为。
int64_t GetNumberOption(const Napi::Object &obj, const char *key, int64_t fallback) {
    if (!obj.Has(key)) {
        return fallback;
    }
    Napi::Value v = obj.Get(key);
    if (!v.IsNumber()) {
        return fallback;
    }
    double d = v.As<Napi::Number>().DoubleValue();
    if (std::isnan(d)) {
        return fallback;
    }
    return static_cast<int64_t>(d);
}

// 读取字符串参数：字段缺失时返回 fallback。
// 直接 ToString() 拿 undefined 会得到字面量 "undefined" 并被当成真实参数传给 sdk。
std::string GetStringOption(const Napi::Object &obj, const char *key, const char *fallback) {
    if (!obj.Has(key)) {
        return fallback;
    }
    Napi::Value v = obj.Get(key);
    if (v.IsUndefined() || v.IsNull()) {
        return fallback;
    }
    return v.ToString().Utf8Value();
}

// 解密失败最常见的原因是私钥版本对不上，publickey_ver 指明这条消息该用哪个版本的私钥。
// 注意只打印版本号，不要把 private_key 打进日志。
void LogDecryptKeyFailure(const rapidjson::Value &item) {
    printf("%sdecrypt encrypt_random_key failed, seq:%lld publickey_ver:%lld, "
           "请确认 private_key 是该 publickey_ver 对应版本的私钥\n",
           ERROR_PREFIX.c_str(),
           static_cast<long long>(GetInt64Member(item, "seq", -1)),
           static_cast<long long>(GetInt64Member(item, "publickey_ver", -1)));
}

}  // namespace

Napi::Object WeWorkChat::Init(Napi::Env env, Napi::Object exports) {
  Napi::Function func =
      DefineClass(env,
                  "WeWorkChat",
                  {
                   InstanceMethod("getMediaData", &WeWorkChat::GetMediaData),
                   InstanceMethod("fetchData", &WeWorkChat::StartFetchData),
                   InstanceMethod("getChatData", &WeWorkChat::GetChat),
                   InstanceMethod("stopFetch", &WeWorkChat::EndFetchData)});

  Napi::FunctionReference* constructor = new Napi::FunctionReference();
  *constructor = Napi::Persistent(func);
  env.SetInstanceData(constructor);

  exports.Set("WeWorkChat", func);
  return exports;
}

int WeWorkChat::initSdk(const Napi::CallbackInfo& info){
    int ret = 0;
    // new sdk api
    this->sdk_ = ::NewSdk();
    if (this->sdk_ == nullptr) {
        printf("%sNewSdk returned null\n", ERROR_PREFIX.c_str());
        Napi::TypeError::New(info.Env(), "Create WeWorkFinance sdk error.")
            .ThrowAsJavaScriptException();
        return -1;
    }
    ret = ::Init(this->sdk_, this->corpid_.c_str(), this->secret_.c_str());
    if (ret != 0)
    {
        printf("%sinit sdk err ret:%d\n", ERROR_PREFIX.c_str(), ret);
        // 初始化失败后不要留着半初始化的指针，否则后续任何调用都是崩溃
        ::DestroySdk(this->sdk_);
        this->sdk_ = nullptr;
        this->sdk_destroyed_ = true;
        Napi::TypeError::New(info.Env(), "Init WeWorkFinance sdk error.")
            .ThrowAsJavaScriptException();
        return  -1;
    }
    return 0;
}

bool WeWorkChat::ensureSdk(Napi::Env env) {
    if (this->sdk_ == nullptr) {
        Napi::Error::New(env, ERROR_PREFIX + "sdk unavailable: init failed or stopFetch() already released it")
            .ThrowAsJavaScriptException();
        return false;
    }
    return true;
}

void WeWorkChat::destroySdk() {
    bool expected = false;
    // 重复调用 stopFetch 会 double free sdk，用 CAS 保证只释放一次
    if (this->sdk_ != nullptr && this->sdk_destroyed_.compare_exchange_strong(expected, true)) {
        ::DestroySdk(this->sdk_);
        this->sdk_ = nullptr;
    }
}

void WeWorkChat::ReleaseFetchRef() {
    this->Unref();
}

WeWorkChat::WeWorkChat(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<WeWorkChat>(info) {
        Napi::Env env = info.Env();

        this->sdk_ = nullptr;
        this->seq_ = 0;
        this->end_ = false;
        this->fetching_ = false;
        this->sdk_destroyed_ = false;

        int length = info.Length();

        if (length <= 0 || !info[0].IsObject()) {
            Napi::TypeError::New(env, "Expected one object argument").ThrowAsJavaScriptException();
            return;
        }

        Napi::Object obj = info[0].As<Napi::Object>();

        // 必填参数缺失时直接报错，不要带着空的 corpid/secret 去 Init
        const char *required[] = {"corpid", "secret", "private_key"};
        for (const char *key : required) {
            if (!obj.Has(key) || !obj.Get(key).IsString()) {
                Napi::TypeError::New(env, std::string("Missing or invalid option: ") + key)
                    .ThrowAsJavaScriptException();
                return;
            }
        }

        this->corpid_ = obj.Get("corpid").As<Napi::String>().Utf8Value();
        this->secret_ = obj.Get("secret").As<Napi::String>().Utf8Value();
        this->private_key_ = obj.Get("private_key").As<Napi::String>().Utf8Value();

        int64_t seq = GetNumberOption(obj, "seq", 0);
        this->seq_ = seq < 0 ? 0 : seq;

        this->initSdk(info);
}

WeWorkChat::~WeWorkChat() {
    // 用户忘了调 stopFetch 时兜底释放 sdk。
    this->end_ = true;
    if (this->fetching_.load()) {
        // 正常情况下 fetch 期间对象被 Ref 住不会走到这里，但进程/env 退出时
        // 析构会被强制执行。此时后台线程可能还在用 sdk，宁可漏掉一次释放，
        // 也不能让它拿到野指针。
        return;
    }
    this->destroySdk();
}


Napi::Value WeWorkChat::EndFetchData(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    this->end_ = true;

    // 必须等后台线程真正退出再 DestroySdk。原来只 sleep 800ms，而一次
    // GetChatData 最长要等 30s，sdk 被提前释放后线程继续用它就是
    // Segmentation fault (core dumped)。
    int waited = 0;
    while (this->fetching_.load() && waited < kStopWaitMs) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        waited += 50;
    }

    if (this->fetching_.load()) {
        printf("%sstopFetch: fetch thread still running after %dms, skip DestroySdk\n",
               ERROR_PREFIX.c_str(), waited);
        return Napi::Number::New(env, static_cast<double>(this->seq_.load()));
    }

    // sdk 由用户手动释放
    this->destroySdk();
    return Napi::Number::New(env, static_cast<double>(this->seq_.load()));
}

Napi::Value WeWorkChat::GetChat(const Napi::CallbackInfo& info){
    Napi::Env env = info.Env();

    int length = info.Length();

    if (length <= 0 || !info[0].IsObject()) {
        Napi::TypeError::New(env, "Expected one object argument").ThrowAsJavaScriptException();
        // 编译时定义了 NAPI_DISABLE_CPP_EXCEPTIONS，ThrowAsJavaScriptException 不会中断
        // C++ 执行流，这里不 return 的话下一行就会把非对象当对象用
        return env.Null();
    }

    if (!this->ensureSdk(env)) {
        return env.Null();
    }

    Napi::Object obj = info[0].As<Napi::Object>();

    // 之前这行把 max_results 读进了一个叫 sdk_fileid 且没人使用的字符串，
    // 下面调用 GetChatData 时用的是常量 1000，所以传什么都没效果
    int64_t max_results = GetNumberOption(obj, "max_results", kMaxResults);
    if (max_results <= 0 || max_results > kMaxResults) {
        max_results = kMaxResults;
    }
    std::int64_t seq = GetNumberOption(obj, "seq", 0);
    if (seq < 0) seq = 0;
    std::int64_t timeout = GetNumberOption(obj, "timeout", kDefaultTimeout);
    if (timeout <= 0) timeout = kDefaultTimeout;

    SliceGuard chatDatas;
    if (!chatDatas.valid()) {
        Napi::Error::New(env, ERROR_PREFIX + "NewSlice failed").ThrowAsJavaScriptException();
        return env.Null();
    }

    // getchatdata api
    const int numOfRetries = 3;
    int cnt = 1;
    int ret = 0;
    do {
        ret = GetChatData(this->sdk_, static_cast<unsigned long long>(seq),
                          static_cast<unsigned int>(max_results), "", "",
                          static_cast<int>(timeout), chatDatas.get());
        if   (ret >= 10001 && ret <= 10003){
            //cout << "\t try number#" << cnt <<" fail \n";
            std::this_thread::sleep_for(std::chrono::seconds(1));
            ++cnt;
        } else{
            break;
        }
    }while (cnt <= numOfRetries);

    if (ret != 0)
    {
        printf("%sGetChatData err ret:%d\n",ERROR_PREFIX.c_str(), ret);
        char errMsg[256];
        snprintf(errMsg, sizeof(errMsg), "%sGetChatData err ret:%d",ERROR_PREFIX.c_str(), ret);
        Napi::Error::New(env, errMsg).ThrowAsJavaScriptException();
        return env.Null();
    }

    char *data = GetContentFromSlice(chatDatas.get());
    if (data == nullptr) {
        Napi::Error::New(env, ERROR_PREFIX + "GetContentFromSlice returned null").ThrowAsJavaScriptException();
        return env.Null();
    }
    // parse data
    rapidjson::Document doc;
    if (doc.Parse(data).HasParseError())
    {
        printf("%sparse json data error,data:%s\n",ERROR_PREFIX.c_str(), data);
        printf("%sparse error: (%d:%zu)%s\n", ERROR_PREFIX.c_str(), doc.GetParseError(), doc.GetErrorOffset(), rapidjson::GetParseError_En(doc.GetParseError()));

        Napi::Error::New(env, "parse json data error").ThrowAsJavaScriptException();
        return env.Null();
    }
    if (doc.HasMember("errcode"))
    {
        int errcode = static_cast<int>(GetInt64Member(doc, "errcode", 0));
        if (errcode != 0)
        {
            const char *errMsg = GetStringMember(doc, "errmsg", "unknown error");
            printf("%sget chat message error:%s.\n",ERROR_PREFIX.c_str(), errMsg);
            char msg[256];
            snprintf(msg, sizeof(msg), "get chat message error, errcode:%d errmsg:%s", errcode, errMsg);
            Napi::Error::New(env, msg).ThrowAsJavaScriptException();
            return env.Null();
        }
    }

    Napi::Object retObj = Napi::Object::New(env);

    const rapidjson::Value *chatDataPtr = FindMember(doc, "chatdata");
    if (chatDataPtr == nullptr || !chatDataPtr->IsArray()) {
        // errcode 为 0 但没有 chatdata（比如已经拉到最新），按空结果返回，
        // 不要让 doc["chatdata"] 在 Release 构建下直接崩掉
        retObj.Set("last_seq", Napi::Number::New(env, 0));
        retObj.Set("data", Napi::Array::New(env, 0));
        return retObj;
    }
    const rapidjson::Value &chatData = *chatDataPtr;

    unsigned int dataSize = chatData.Size();
    Napi::Array data_array = Napi::Array::New(env, dataSize);

    int64_t last_seq = 0;
    if (dataSize > 0) {
        last_seq = GetInt64Member(chatData[dataSize - 1], "seq", 0);
    }
    for (SizeType i = 0; i < dataSize; ++i)
    {
        const rapidjson::Value &item = chatData[i];
        string encryptRandomKey = GetStringMember(item, "encrypt_random_key", "");
        string encryptChatMsg = GetStringMember(item, "encrypt_chat_msg", "");
        if (encryptRandomKey.empty() || encryptChatMsg.empty()) {
            printf("%sskip malformed chatdata item, seq:%lld\n", ERROR_PREFIX.c_str(),
                   static_cast<long long>(GetInt64Member(item, "seq", -1)));
            continue;
        }
        string encrypt_key = rsa_pri_decrypt(encryptRandomKey, this->private_key_.c_str());
        if (encrypt_key.length()==0) {
            LogDecryptKeyFailure(item);
            continue;
        }
        SliceGuard slice_msg;
        if (!slice_msg.valid()) {
            printf("%sNewSlice failed\n", ERROR_PREFIX.c_str());
            continue;
        }

        int decRet = DecryptData(encrypt_key.c_str(), encryptChatMsg.c_str(), slice_msg.get());
        if (decRet != 0){
            cout << ERROR_PREFIX <<"Decrypt Data error:"<<decRet<< endl;
            continue;
        }

        char *msg_data = GetContentFromSlice(slice_msg.get());
        if (msg_data == nullptr) {
            printf("%sGetContentFromSlice returned null\n", ERROR_PREFIX.c_str());
            continue;
        }
        data_array[i] = Napi::String::New(env, msg_data);
    }
    retObj.Set("last_seq", Napi::Number::New(env, static_cast<double>(last_seq)));
    retObj.Set("data", data_array);
    return retObj;
}

void* WeWorkChat::fetchData(TsfnContext *context, void *this__) {
    WeWorkChat * this_ =  static_cast<WeWorkChat*>(this__);

    while (true) {
        if (this_->end_.load()){
            cout << "End fetch data:"<<this_->seq_.load()<< endl;
            break;
        }
        // 微信限制频率为最高100ms/每次
        std::this_thread::sleep_for(std::chrono::milliseconds(150));

        SliceGuard chatDatas;
        if (!chatDatas.valid()) {
            printf("%sNewSlice failed\n", ERROR_PREFIX.c_str());
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        // getchatdata api
        const int numOfRetries = 3;
        int cnt = 1;
        int ret = 0;
        do {
            ret = GetChatData(this_->sdk_, static_cast<unsigned long long>(this_->seq_.load()),
                              static_cast<unsigned int>(kMaxResults), "", "",
                              kFetchTimeout, chatDatas.get());
            if   (ret >= 10001 && ret <= 10003){
                //cout << "\t try number#" << cnt <<" fail \n";
                std::this_thread::sleep_for(std::chrono::seconds(1));
                ++cnt;
            } else{
                break;
            }
        }while (cnt <= numOfRetries);

        if (ret != 0)
        {
            printf("%sGetChatData err ret:%d\n",ERROR_PREFIX.c_str(), ret);
            // SliceGuard 析构会释放，之前这里 continue 会漏掉 FreeSlice，
            // 每次错误响应泄漏一个 Slice_t，长时间运行会不断增长
            continue;
        }

        char *data = GetContentFromSlice(chatDatas.get());
        if (data == nullptr) {
            printf("%sGetContentFromSlice returned null\n", ERROR_PREFIX.c_str());
            continue;
        }

        int64_t seq = this_->parseJsonData(context,data);
        if (seq <0) {
            continue;
        }
    }

    // 先标记线程已退出，stopFetch 看到这个标记后才会去 DestroySdk
    this_->fetching_ = false;
    context->tsfn.Release();
    return 0;
}

Napi::Value WeWorkChat::StartFetchData(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() <= 0 || !info[0].IsFunction()) {
        Napi::TypeError::New(env, "Expected a callback function").ThrowAsJavaScriptException();
        return env.Null();
    }

    if (!this->ensureSdk(env)) {
        return env.Null();
    }

    // 重复调用会起多个线程同时改写 seq_，而且只有一个线程会被 join
    if (this->fetching_.load()) {
        Napi::Error::New(env, ERROR_PREFIX + "fetchData is already running, call stopFetch() first")
            .ThrowAsJavaScriptException();
        return env.Null();
    }

    this->end_ = false;
    this->fetching_ = true;

    auto tsContext = new TsfnContext(env);
    tsContext->owner = this;

      // Create a new ThreadSafeFunction.
    tsContext->tsfn =
          Napi::ThreadSafeFunction::New(env,                    // Environment
                                  info[0].As<Napi::Function>(), // JS function from caller
                                  "TSFN_FETCHDATA",                 // Resource name
                                  0,        // Max queue size (0 = unlimited).
                                  1,        // Initial thread count
                                        tsContext, // Context,
                                  FinalizerCallback, // Finalizer
                                  (void *)nullptr    // Finalizer data
          );

    // 后台线程直接使用 this，期间必须阻止 JS 对象被 GC 回收，
    // 否则线程会访问已释放的实例
    this->Ref();
    tsContext->nativeThread = std::thread(fetchData, tsContext, this);

    return tsContext->deferred.Promise();
}

int64_t WeWorkChat::parseJsonData(TsfnContext *context,const char *data){
    auto callback = [](Napi::Env env, Napi::Function jsCallback, MsgData *msg) {
        if (msg == nullptr) {
            return;
        }
        // env 为空表示 tsfn 正在销毁，此时不能再回调 JS，但仍要把内存还回去
        if (env != nullptr && msg->msg_data != nullptr) {
            jsCallback.Call({Napi::String::New(env, msg->msg_data)});
        }
        FreeSlice(msg->slice_msg);
        delete msg;
    };

    rapidjson::Document doc;
    if (doc.Parse(data).HasParseError())
    {
        printf("%sparse json data error,data:%s\n",ERROR_PREFIX.c_str(), data);
        printf("%sparse error: (%d:%zu)%s\n", ERROR_PREFIX.c_str(), doc.GetParseError(), doc.GetErrorOffset(), rapidjson::GetParseError_En(doc.GetParseError()));

        return -1;
    }
    if (doc.HasMember("errcode"))
    {
        int errcode = static_cast<int>(GetInt64Member(doc, "errcode", 0));
        if (errcode != 0)
        {
            printf("%sget chat message error:%s.\n",ERROR_PREFIX.c_str(),
                   GetStringMember(doc, "errmsg", "unknown error"));
            return -1;
        }
    }

    const rapidjson::Value *chatDataPtr = FindMember(doc, "chatdata");
    if (chatDataPtr == nullptr || !chatDataPtr->IsArray()) {
        // 没有新消息时按空批次处理，不能直接 doc["chatdata"]
        return this->seq_.load();
    }
    const rapidjson::Value &chatData = *chatDataPtr;

    for (SizeType i = 0; i < chatData.Size(); ++i)
    {
        // 一批最多 1000 条、每条 sleep 80ms，不检查 end_ 的话 stopFetch 之后
        // 还要跑 80s 才停得下来
        if (this->end_.load()) {
            break;
        }

        const rapidjson::Value &item = chatData[i];
        int64_t seq = GetInt64Member(item, "seq", -1);
        if (seq < 0) {
            printf("%sskip chatdata item without seq\n", ERROR_PREFIX.c_str());
            continue;
        }
        this->seq_ = seq;

        string encryptRandomKey = GetStringMember(item, "encrypt_random_key", "");
        string encryptChatMsg = GetStringMember(item, "encrypt_chat_msg", "");
        if (encryptRandomKey.empty() || encryptChatMsg.empty()) {
            printf("%sskip malformed chatdata item, seq:%lld\n", ERROR_PREFIX.c_str(),
                   static_cast<long long>(seq));
            continue;
        }
        string encrypt_key = rsa_pri_decrypt(encryptRandomKey, this->private_key_.c_str());
        if (encrypt_key.length()==0) {
            LogDecryptKeyFailure(item);
            continue;
        }
        SliceGuard slice_msg;
        if (!slice_msg.valid()) {
            printf("%sNewSlice failed\n", ERROR_PREFIX.c_str());
            continue;
        }

        int ret = DecryptData(encrypt_key.c_str(), encryptChatMsg.c_str(), slice_msg.get());
        if (ret != 0){
            cout << ERROR_PREFIX <<"Decrypt Data error:"<<ret<< endl;
            continue;
        }

        char *msg_data = GetContentFromSlice(slice_msg.get());
        if (msg_data == nullptr) {
            printf("%sGetContentFromSlice returned null\n", ERROR_PREFIX.c_str());
            continue;
        }

        MsgData *theData = new MsgData();
        theData->msg_data = msg_data;
        // 所有权移交给回调：确保执行完 callback 再释放 slice_msg
        theData->slice_msg = slice_msg.release();

        napi_status status =
            context->tsfn.BlockingCall(theData, callback);

        if (status != napi_ok) {
            FreeSlice(theData->slice_msg);
            delete theData;
            Napi::Error::Fatal("parseJsonData", "Napi::ThreadSafeNapi::Function.BlockingCall() failed");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
    }
    return this->seq_.load();
}

Napi::Value WeWorkChat::GetMediaData(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    int length = info.Length();

    if (length <= 0 || !info[0].IsObject()) {
        Napi::TypeError::New(env, "Expected one object argument").ThrowAsJavaScriptException();
        // 同 GetChat，这里必须 return，否则下一行会把非对象当对象解引用
        return env.Null();
    }

    if (!this->ensureSdk(env)) {
        return env.Null();
    }

    Napi::Object obj = info[0].As<Napi::Object>();

    std::string sdk_fileid = GetStringOption(obj, "sdk_fileid", "");
    // index_buf 是可选参数，之前不传时 ToString() 会得到字面量 "undefined"
    // 并被当成真实的分片索引传给 sdk
    std::string index_buf = GetStringOption(obj, "index_buf", "");

    if (sdk_fileid.empty()) {
        Napi::TypeError::New(env, "Missing or invalid option: sdk_fileid").ThrowAsJavaScriptException();
        return env.Null();
    }

    Napi::Function cb;
    bool isCbFunction = false;
    if (info.Length() > 1 && info[1].IsFunction()){
        cb = info[1].As<Napi::Function>();
        isCbFunction = true;
    }

    // 记得释放
    MediaData_t* media = NewMediaData();
    if (media == nullptr) {
        Napi::Error::New(env, ERROR_PREFIX + "NewMediaData failed").ThrowAsJavaScriptException();
        return env.Null();
    }

    const int numOfRetries = 3;
    int cnt = 1;
    int ret = 0;
    do {
        ret = ::GetMediaData(this->sdk_, index_buf.c_str(),sdk_fileid.c_str(), "", "",30, media);
        if (ret >= 10001 && ret <= 10003){
            cout << "\t try number#" << cnt <<" fail \n";
            std::this_thread::sleep_for(std::chrono::seconds(1));
            ++cnt;
        } else{
            break;
        }
    }while (cnt <= numOfRetries);

    if (ret != 0) {
        printf("%sGetMediaData err ret:%d\n",ERROR_PREFIX.c_str(), ret);
        ::FreeMediaData(media);
        if (isCbFunction){
            cb.Call(env.Global(), {Napi::String::New(env, "GetMediaData err")});
            return Napi::Number::New(env,-1);
        } else{
            char errMsg[128];
            snprintf(errMsg, sizeof(errMsg), "Get media data error,errorcode:%d", ret);
            Napi::Error::New(env, errMsg).ThrowAsJavaScriptException();
            return env.Null();
        }

    }

    char *media_data = ::GetData(media);
    int media_data_len = ::GetDataLen(media);
    if (media_data == nullptr || media_data_len < 0) {
        printf("%sGetMediaData returned empty buffer\n", ERROR_PREFIX.c_str());
        ::FreeMediaData(media);
        if (isCbFunction){
            cb.Call(env.Global(), {Napi::String::New(env, "GetMediaData returned empty buffer")});
            return Napi::Number::New(env,-1);
        }
        Napi::Error::New(env, ERROR_PREFIX + "GetMediaData returned empty buffer").ThrowAsJavaScriptException();
        return env.Null();
    }

    std:: string out_index_buf = "";
    // ArrayBuffer 接管 media 的生命周期，由 MediaDataFinalizerCallback 释放
    Napi::ArrayBuffer buf_data = Napi::ArrayBuffer::New(env, media_data, static_cast<size_t>(media_data_len),
                                                       MediaDataFinalizerCallback, media);
    bool is_finish;
    if(IsMediaDataFinish(media)==1)
    {
        is_finish = true;
        out_index_buf = "";
       //break;
    } else {
        const char *next_index = ::GetOutIndexBuf(media);
        out_index_buf = next_index == nullptr ? "" : next_index;
        is_finish = false;
    }

    Napi::Object retObj = Napi::Object::New(env);
    retObj.Set("is_finished", Napi::Boolean::New(env,is_finish));
    retObj.Set("buf_index",Napi::String::New(env,out_index_buf));
    retObj.Set("data", buf_data);

    if (isCbFunction){
        cb.Call(env.Global(), {env.Null(),retObj});
        return env.Null();
    } else{
        return retObj;
    }

}


std::string rsa_pri_decrypt(const std::string &cipherText,const char *priKey)
{
    std::string random_key = decode64(cipherText);
    //printf("randkey decodebase64:%s\n", random_key.c_str());
    std::string strRet;
    RSA *rsa = RSA_new();
    BIO *keybio;
    keybio = BIO_new_mem_buf(priKey, -1);
    // 此处有三种方法
    // 1, 读取内存里生成的密钥对，再从内存生成rsa
    // 2, 读取磁盘里生成的密钥对文本文件，在从内存生成rsa
    // 3，直接从读取文件指针生成rsa
    rsa = PEM_read_bio_RSAPrivateKey(keybio, &rsa, NULL, NULL);
    if(rsa == NULL)
    {
        printf("%sFailed to create RSA.\n",ERROR_PREFIX.c_str());
        BIO_free_all(keybio);
        return "";
    }
    int len = RSA_size(rsa);
    char *decryptedText = (char *)malloc(len + 1);
    if (decryptedText == NULL)
    {
        BIO_free_all(keybio);
        RSA_free(rsa);
        return "";
    }
    memset(decryptedText, 0, len + 1);

    // 解密函数
    int ret = RSA_private_decrypt(random_key.length(), (const unsigned char *)random_key.c_str(), (unsigned char *)decryptedText, rsa, RSA_PKCS1_PADDING);
    if (ret >= 0)
        strRet = std::string(decryptedText, ret);

    // 释放内存
    free(decryptedText);
    BIO_free_all(keybio);
    RSA_free(rsa);

    return strRet;
}

std::string decode64(const std::string &ascdata)
{
   using std::string;
   string retval;
   const string::const_iterator last = ascdata.end();
   int bits_collected = 0;
   unsigned int accumulator = 0;

   for (string::const_iterator i = ascdata.begin(); i != last; ++i) {
      const int c = *i;
      if (std::isspace(c) || c == '=') {
         continue;
      }
      if ((c > 127) || (c < 0) || (reverse_table[c] > 63)) {
         printf("%sThis contains characters not legal in a base64 encoded string.\n",ERROR_PREFIX.c_str());
         return "";
      }
      accumulator = (accumulator << 6) | reverse_table[c];
      bits_collected += 6;
      if (bits_collected >= 8) {
         bits_collected -= 8;
         retval += static_cast<char>((accumulator >> bits_collected) & 0xffu);
      }
   }
   return retval;
}
