#pragma once

// RawrXD Sovereign command identifiers (keep them in the high range to avoid Win32 clashes)
#define ID_FILE_NEW              0xE100
#define ID_FILE_OPEN             0xE101
#define ID_FILE_SAVE             0xE102
#define ID_FILE_SAVE_AS          0xE103
#define ID_FILE_EXIT             0xE1FF

#define ID_VIEW_TOGGLE_EXPLORER  0xE110
#define ID_VIEW_TOGGLE_CHAT      0xE111
#define ID_VIEW_TOGGLE_SETTINGS  0xE112
#define ID_VIEW_TOGGLE_FULLSCREEN 0xE113

#define ID_TERMINAL_NEW          0xE500
#define ID_TERMINAL_RUN_BUILD     0xE501

#define ID_HELP_DOCUMENTATION    0xE400
#define ID_HELP_ABOUT            0xE401

#define ID_AI_COMPLETE           0xE200
#define ID_AI_AGENT_MODE         0xE201
#define ID_AI_CHAT               0xE202
#define ID_AI_REWRITE            0xE203

#define ID_MODEL_LOAD            0xE300
#define ID_MODEL_UNLOAD          0xE301
#define ID_MODEL_QUANTIZE        0xE302

#define ID_DEBUG_INJECT_DLL      0xEF00
