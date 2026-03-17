#pragma once

// Menu IDs for RawrXD IDE
#define IDM_FILE_NEW            101
#define IDM_FILE_OPEN           102
#define IDM_FILE_SAVE           103
#define IDM_FILE_SAVEAS         104
#define IDM_FILE_AUTOSAVE       105
#define IDM_FILE_CLOSE_FOLDER   106
#define IDM_FILE_EXIT           107
#define IDM_FILE_OPEN_FOLDER    108
#define IDM_FILE_NEW_WINDOW     109
#define IDM_FILE_CLOSE_TAB      110

#define IDM_EDIT_UNDO           201
#define IDM_EDIT_REDO           202
#define IDM_EDIT_CUT            203
#define IDM_EDIT_COPY           204
#define IDM_EDIT_PASTE          205
#define IDM_EDIT_FIND           206
#define IDM_EDIT_REPLACE        207
#define IDM_EDIT_SELECTALL      208
#define IDM_EDIT_MULTICURSOR_ADD    209
#define IDM_EDIT_MULTICURSOR_REMOVE 210
#define IDM_EDIT_GOTO_LINE      211

#define IDM_VIEW_TOGGLE_SIDEBAR     301
#define IDM_VIEW_TOGGLE_TERMINAL    302
#define IDM_VIEW_TOGGLE_OUTPUT      303
#define IDM_VIEW_TOGGLE_FULLSCREEN  304
#define IDM_VIEW_ZOOM_IN            305
#define IDM_VIEW_ZOOM_OUT           306
#define IDM_VIEW_ZOOM_RESET         307

#define IDM_AI_INLINE_COMPLETE      401
#define IDM_AI_CHAT_MODE            402
#define IDM_AI_EXPLAIN_CODE         403
#define IDM_AI_REFACTOR             404
#define IDM_AI_GENERATE_TESTS       405
#define IDM_AI_GENERATE_DOCS        406
#define IDM_AI_FIX_ERRORS           407
#define IDM_AI_OPTIMIZE_CODE        408
#define IDM_AI_MODEL_SELECT         409

#define IDM_TOOLS_COMMAND_PALETTE   501
#define IDM_TOOLS_SETTINGS          502
#define IDM_TOOLS_EXTENSIONS        503
#define IDM_TOOLS_TERMINAL          504
#define IDM_TOOLS_BUILD             505
#define IDM_TOOLS_DEBUG             506

#define IDM_HELP_DOCS               601
#define IDM_HELP_ABOUT              602
#define IDM_HELP_SHORTCUTS          603

// Control IDs
#define ID_EDITOR_CONTROL           1001
#define ID_FILE_EXPLORER            1002
#define ID_TERMINAL_PANEL           1003
#define ID_OUTPUT_PANEL             1004
#define ID_TAB_CONTROL              1005
#define ID_TOOLBAR                  1006
#define ID_STATUSBAR                1007
#define ID_COMMAND_PALETTE          1008
#define ID_AI_CHAT_PANEL            1009
#define ID_AUTOCOMPLETE_LIST        1010
#define ID_PARAMETER_HINT           1011
#define ID_MARKETPLACE_PANEL        1012

// AI Model Type Constants
#define AI_MODEL_CURSOR_SMALL       0
#define AI_MODEL_CURSOR_MEDIUM      1
#define AI_MODEL_CURSOR_LARGE       2
#define AI_MODEL_COPILOT            3
#define AI_MODEL_LOCAL_GGUF         4
#define AI_MODEL_OLLAMA             5

// Editor Behavior
#define AUTOCOMPLETE_TRIGGER_DELAY  300  // ms
#define AI_INLINE_TRIGGER_DELAY     500  // ms
#define SYNTAX_HIGHLIGHT_BATCH      100  // lines per batch
#define MAX_UNDO_LEVELS             1000
#define MAX_TABS                    50

// Theme Colors (RGB)
#define COLOR_BACKGROUND            RGB(30, 30, 30)
#define COLOR_FOREGROUND            RGB(220, 220, 220)
#define COLOR_SELECTION             RGB(51, 153, 255)
#define COLOR_COMMENT               RGB(106, 153, 85)
#define COLOR_KEYWORD               RGB(86, 156, 214)
#define COLOR_STRING                RGB(206, 145, 120)
#define COLOR_NUMBER                RGB(181, 206, 168)
#define COLOR_FUNCTION              RGB(220, 220, 170)
#define COLOR_AI_SUGGESTION         RGB(100, 100, 255)
