{
  "targets": [
    {
      "target_name": "rgtp",
      "sources": [
        "src/rgtp.cc",
        "../../src/core/rgtp_core.c"
      ],
      "include_dirs": [
        "<!(node -e \"require('node-addon-api').include\")",
        "../../include"
      ],
      "defines": [
        "NAPI_CPP_EXCEPTIONS"
      ],
      "cflags_cc": [
        "-std=c++17"
      ],
      "conditions": [
        ["OS=='win'", {
          "sources": [
            "src/win_delay_load_hook.cc"
          ],
          "msvs_settings": {
            "VCCLCompilerTool": {
              "ExceptionHandling": 1
            }
          }
        }]
      ]
    }
  ]
}