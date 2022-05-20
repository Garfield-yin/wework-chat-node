{
  "targets": [
    {
      "target_name": "wework",
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "libraries": ["<(module_root_dir)/lib/libWeWorkFinanceSdk_C.so"],
      "sources": [ "main.cc","wework.cc","include/wework/wework.h"],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
      ],
      'defines': [ 'NAPI_DISABLE_CPP_EXCEPTIONS' ]
    }
  ]
}
