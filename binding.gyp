{
  "targets": [
    {
      "target_name": "wework",
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "sources": [ "main.cc","wework.cc","include/wework/wework.h"],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")"
      ],
      "defines": [ "NAPI_DISABLE_CPP_EXCEPTIONS" ],
      "conditions": [
        ["target_arch=='arm64'", {
          "libraries": [ "<(module_root_dir)/lib/arm/libWeWorkFinanceSdk_C.so" ],
          "include_dirs": [ "include/wework/arm" ]
        }, {
          "libraries": [ "<(module_root_dir)/lib/x86/libWeWorkFinanceSdk_C.so" ],
          "include_dirs": [ "include/wework/x86" ]
        }]
      ]
    }
  ]
}
