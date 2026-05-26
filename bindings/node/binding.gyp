{
  "targets": [
    {
      "target_name": "rgtp",
      "sources": [
        "src/rgtp.cc"
      ],
      "include_dirs": [
        "<!(node -e \"require('node-addon-api').include\")",
        "../../include"
      ],
      "defines": [
        "NAPI_CPP_EXCEPTIONS"
      ],
      "cflags!":    [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "cflags_cc":  [ "-std=c++17" ],
      "conditions": [
        ["OS=='linux'", {
          "libraries": [ "-lrgtp", "-lsodium", "-lpthread" ],
          "library_dirs": [ "/usr/local/lib", "/usr/lib" ]
        }],
        ["OS=='mac'", {
          "libraries": [ "-lrgtp", "-lsodium" ],
          "library_dirs": [ "/usr/local/lib", "/opt/homebrew/lib" ],
          "xcode_settings": {
            "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
            "CLANG_CXX_LANGUAGE_STANDARD": "c++17",
            "MACOSX_DEPLOYMENT_TARGET": "11.0"
          }
        }],
        ["OS=='win'", {
          "libraries": [ "rgtp.lib", "libsodium.lib", "ws2_32.lib", "iphlpapi.lib" ],
          "msvs_settings": {
            "VCCLCompilerTool": {
              "ExceptionHandling": 1,
              "AdditionalOptions": [ "/std:c++17" ]
            }
          }
        }]
      ]
    }
  ]
}
