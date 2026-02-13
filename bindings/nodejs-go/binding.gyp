{
  "targets": [
    {
      "target_name": "rgtp_bridge",
      "sources": [
        "src/rgtp_bridge.cc"
      ],
      "libraries": [
        "../go/rgtp_bridge.lib"
      ],
      "include_dirs": [
        "<!(node -e \"require('node-addon-api').include\")",
        "../go"
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