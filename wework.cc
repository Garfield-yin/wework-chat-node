#include <iostream>
#include <fstream>
#include <string>
#include <cassert>

#include <limits>
#include <thread>
#include <mutex>
#include <chrono>

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

//WeWorkFinanceSdk_t *WeWorkChat::sdk_;
//std::string WeWorkChat::private_key_;
//int64_t WeWorkChat::seq_;
//bool WeWorkChat::end_;
//std::mutex WeWorkChat::mtx_;

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

const int max_results = 1000;

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
    ret = ::Init(this->sdk_, this->corpid_.c_str(), this->secret_.c_str());
    if (ret != 0)
    {
        printf("%sinit sdk err ret:%d\n", ERROR_PREFIX.c_str(), ret);
        Napi::TypeError::New(info.Env(), "Init WeWorkFinance sdk error.")
            .ThrowAsJavaScriptException();
        return  -1;
    }
    return 0;
}

WeWorkChat::WeWorkChat(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<WeWorkChat>(info) {
        Napi::Env env = info.Env();

        int length = info.Length();

        if (length <= 0 || !info[0].IsObject()) {
            Napi::TypeError::New(env, "Expected one object argument").ThrowAsJavaScriptException();
            return;
        }

        Napi::Object obj = info[0].As<Napi::Object>();
        //value.ToObject()
        this->corpid_ = obj.Get("corpid").ToString();
        this->secret_ = obj.Get("secret").ToString();
        this->private_key_ = obj.Get("private_key").ToString();
        this->seq_ = obj.Get("seq").ToNumber();
        
        //cout <<"corpid:"<< this->corpid_<<",secret:"<< this->secret_<<",seq:"<<this->seq_ << endl;
        
        this->end_ = false;
        this->initSdk(info);
}


Napi::Value WeWorkChat::EndFetchData(const Napi::CallbackInfo& info) {
    this->end_ = true;
    // sleep 一下，确保正在执行的线程执行完毕
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    // sdk 由用户手动释放
    DestroySdk(this->sdk_);
    return Napi::Number::New(info.Env(), this->seq_);
}

Napi::Value WeWorkChat::GetChat(const Napi::CallbackInfo& info){
    Napi::Env env = info.Env();

    int length = info.Length();

    if (length <= 0 || !info[0].IsObject()) {
        Napi::TypeError::New(env, "Expected one object argument").ThrowAsJavaScriptException();
    }

    Napi::Object obj = info[0].As<Napi::Object>();
   
    std::string sdk_fileid = obj.Get("max_results").ToString();
    std::int64_t seq = obj.Get("seq").ToNumber();
    std::int64_t timeout = obj.Get("timeout").ToNumber();
    if (!seq) seq= 0;
    
    Slice_t *chatDatas = NewSlice();
    // getchatdata api
    const int numOfRetries = 3;
    int cnt = 1;
    int ret = 0;
    do {
        ret = GetChatData(this->sdk_, seq, max_results, "", "", timeout, chatDatas);
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
        sprintf(errMsg, "%sGetChatData err ret:%d\n",ERROR_PREFIX.c_str(), ret);
        Napi::Error::New(env, errMsg).ThrowAsJavaScriptException();
        return env.Null();
    }
    
    char *data = GetContentFromSlice(chatDatas);
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
        int errcode = doc["errcode"].GetInt();
        if (errcode != 0)
        {
            string errMsg = doc["errmsg"].GetString();
            printf("%sget chat message error:%s.\n",ERROR_PREFIX.c_str(), errMsg.c_str());
            Napi::Error::New(env, "get chat message error").ThrowAsJavaScriptException();
            return env.Null();
        }
    }
    const rapidjson::Value &chatData = doc["chatdata"];
    Napi::Array data_array = Napi::Array::New(info.Env(), chatData.Size());
    Napi::Object retObj = Napi::Object::New(env);
    
    unsigned int dataSize =chatData.Size();
    int64_t last_seq = 0;
    if (dataSize >0) {
        last_seq =chatData[dataSize-1]["seq"].GetInt64();
    }
    for (SizeType i = 0; i < dataSize; ++i)
    {
        //cout << "current seq:" <<chatData[i]["seq"].GetInt64()<<endl;
        string encryptRandomKey = chatData[i]["encrypt_random_key"].GetString();
        //cout << "encrypt_random_key: " << encryptRandomKey << endl;
        string encryptChatMsg = chatData[i]["encrypt_chat_msg"].GetString();
        //cout << "encrypt_chat_msg: " << encryptChatMsg << endl;
        string encrypt_key = rsa_pri_decrypt(encryptRandomKey, this->private_key_.c_str());
        //cout << "encrypt_key: " << encrypt_key << endl;
        if (encrypt_key.length()==0) {
            cout <<"random_key:"<<encryptRandomKey<<endl;
            cout <<"encrypt_chat_msg:"<<encryptChatMsg<<endl;
            cout <<"private_key_:"<<this->private_key_<<endl;
            continue;
        }
        Slice_t *slice_msg = NewSlice();
        
        int ret = DecryptData(encrypt_key.c_str(), encryptChatMsg.c_str(), slice_msg);
        if (ret != 0){
            cout << ERROR_PREFIX <<"Decrypt Data error:"<<ret<< endl;
            continue;
        }
  
        
        char *msg_data = GetContentFromSlice(slice_msg);
        data_array[i] = Napi::String::New(info.Env(), msg_data);
        FreeSlice(slice_msg);
    }
    retObj.Set("last_seq", Napi::Number::New(env,last_seq));
    retObj.Set("data", data_array);
    FreeSlice(chatDatas);
    return retObj;
}

void* WeWorkChat::fetchData(TsfnContext *context, void *this__) {
    WeWorkChat * this_ =  static_cast<WeWorkChat*>(this__);
    
    while (true) {
        if (this_->end_){
            cout << "End fetch data:"<<this_->seq_<< endl;
            break;
        }
        // 微信限制频率为最高100ms/每次
        std::this_thread::sleep_for(std::chrono::milliseconds(150));

        Slice_t *chatDatas = NewSlice();
        // getchatdata api
        const int numOfRetries = 3;
        int cnt = 1;
        int ret = 0;
        do {
            ret = GetChatData(this_->sdk_, this_->seq_, max_results, "", "", 30, chatDatas);
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
            continue;
        }
        
        char *data = GetContentFromSlice(chatDatas);
        
        int64_t seq = this_->parseJsonData(context,data);
        if (seq <0) {
            continue;
        }
        
        FreeSlice(chatDatas);
    }
    
    context->tsfn.Release();
    return 0;
}

Napi::Value WeWorkChat::StartFetchData(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    auto tsContext = new TsfnContext(env);

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
    tsContext->nativeThread = std::thread(fetchData, tsContext, this);
    
    return tsContext->deferred.Promise();
}

int64_t WeWorkChat::parseJsonData(TsfnContext *context,const char *data){
    auto callback = [](Napi::Env env, Napi::Function jsCallback, MsgData *msg) {
        jsCallback.Call({Napi::String::New(env, msg->msg_data)});
        FreeSlice(msg->slice_msg);
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
        int errcode = doc["errcode"].GetInt();
        if (errcode != 0)
        {
            string errMsg = doc["errmsg"].GetString();
            printf("%sget chat message error:%s.\n",ERROR_PREFIX.c_str(), errMsg.c_str());
            return -1;
        }
    }
    const rapidjson::Value &chatData = doc["chatdata"];
  
    for (SizeType i = 0; i < chatData.Size(); ++i)
    {
        int64_t seq = chatData[i]["seq"].GetInt64();
        this->mtx_.lock();
        this->seq_ = seq;
        // cout << "data seq: " << seq << endl;
        this->mtx_.unlock();
        string encryptRandomKey = chatData[i]["encrypt_random_key"].GetString();
        //cout << "encrypt_random_key: " << encryptRandomKey << endl;
        string encryptChatMsg = chatData[i]["encrypt_chat_msg"].GetString();
        //cout << "encrypt_chat_msg: " << encryptChatMsg << endl;
        string encrypt_key = rsa_pri_decrypt(encryptRandomKey, this->private_key_.c_str());
        //cout << "encrypt_key: " << encrypt_key << endl;
        if (encrypt_key.length()==0) {
            cout <<"random_key:"<<encryptRandomKey<<endl;
            cout <<"encrypt_chat_msg:"<<encryptChatMsg<<endl;
            cout <<"private_key_:"<<this->private_key_<<endl;
            continue;
        }
        Slice_t *slice_msg = NewSlice();
        
        int ret = DecryptData(encrypt_key.c_str(), encryptChatMsg.c_str(), slice_msg);
        if (ret != 0){
            cout << ERROR_PREFIX <<"Decrypt Data error:"<<ret<< endl;
            continue;
        }
        //cout << "DecryptData ret: " << ret << endl;
        //int64_t msg_len = GetSliceLen(slice_msg);
        //cout << "msg_len: " << msg_len << endl;
        
        char *msg_data = GetContentFromSlice(slice_msg);
        MsgData *theData = new MsgData();
        theData->msg_data =msg_data;
        theData->slice_msg =slice_msg;
        // 确保执行完 callback 再释放 slice_msg
        napi_status status =
            context->tsfn.BlockingCall(theData, callback);
        
        if (status != napi_ok) {
            FreeSlice(slice_msg);
            Napi::Error::Fatal("parseJsonData", "Napi::ThreadSafeNapi::Function.BlockingCall() failed");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
    }
    return this->seq_;
}

Napi::Value WeWorkChat::GetMediaData(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    int length = info.Length();

    if (length <= 0 || !info[0].IsObject()) {
        Napi::TypeError::New(env, "Expected one object argument").ThrowAsJavaScriptException();
    }

    Napi::Object obj = info[0].As<Napi::Object>();
   
    std::string sdk_fileid = obj.Get("sdk_fileid").ToString();
    std::string index_buf = obj.Get("index_buf").ToString();
    
    Napi::Function cb;
    bool isCbFunction = false;
    if (info[1].IsFunction()){
        cb = info[1].As<Napi::Function>();
        isCbFunction = true;
    }
       
    // 记得释放
    MediaData_t* media = NewMediaData();
    
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
            sprintf(errMsg, "Get media data error,errorcode:%d", ret);
            Napi::Error::New(env, errMsg).ThrowAsJavaScriptException();
            return env.Null();
        }
        
    }
    //int media_data_len = ::GetDataLen(media);
    //int index_len = ::GetIndexLen(media);
     
    //printf("content size:%d isfin:%d outindex:%s index_len:%d\n",media_data_len, is_finish,out_index_buf,index_len);
    //char* media_data = ::GetData(media);
    std:: string out_index_buf = "";
    Napi::ArrayBuffer buf_data = Napi::ArrayBuffer::New(env, GetData(media), GetDataLen(media),MediaDataFinalizerCallback,media);
    bool is_finish;
    if(IsMediaDataFinish(media)==1)
    {
        is_finish = true;
        out_index_buf = "";
       //break;
    } else {
        out_index_buf = ::GetOutIndexBuf(media);
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
        return "";
    }
    int len = RSA_size(rsa);
    char *decryptedText = (char *)malloc(len + 1);
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

