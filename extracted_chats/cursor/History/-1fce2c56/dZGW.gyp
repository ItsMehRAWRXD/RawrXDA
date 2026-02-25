{
  "targets": [
    {
      "target_name": "bigdaddyg_ollama",
      "sources": [
        "bigdaddyg-ollama-http.c"
      ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")"
      ],
      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")"
      ],
      "defines": [
        "NAPI_DISABLE_CPP_EXCEPTIONS"
      ],
      "conditions": [
        [
          "OS=='win'",
          {
            "libraries": [
              "winhttp.lib"
            ],
            "msvs_settings": {
              "VCCLCompilerTool": {
                "ExceptionHandling": 1,
                "Optimization": 3,
                "WholeProgramOptimization": "true",
                "AdditionalOptions": [
                  "/std:c11"
                ]
              },
              "VCLinkerTool": {
                "AdditionalDependencies": [
                  "winhttp.lib"
                ]
              }
            }
          }
        ],
        [
          "OS=='mac'",
          {
            "libraries": [
              "-lcurl"
            ],
            "xcode_settings": {
              "GCC_OPTIMIZATION_LEVEL": "3",
              "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
              "CLANG_CXX_LIBRARY": "libc++",
              "MACOSX_DEPLOYMENT_TARGET": "10.15"
            }
          }
        ],
        [
          "OS=='linux'",
          {
            "libraries": [
              "-lcurl"
            ],
            "cflags": [
              "-fPIC",
              "-O3",
              "-std=c11"
            ],
            "ldflags": [
              "-Wl,-rpath,'$$ORIGIN'"
            ]
          }
        ]
      ]
    }
  ]
}

