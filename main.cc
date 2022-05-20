#include <napi.h>
#include "include/wework/wework.h"

Napi::Object InitAll(Napi::Env env, Napi::Object exports) {
  return WeWorkChat::Init(env, exports);
}

NODE_API_MODULE(wework, InitAll)