;==============================================================================
; plugin_abi.inc - RawrXD Plugin Contract (Stable ABI)
; Version: 1.0 (never changes after deployment)
; Structure: PLUGIN_META + array of AGENT_TOOL
;==============================================================================

;==============================================================================
; PLUGIN METADATA STRUCTURE
;==============================================================================
PLUGIN_META STRUCT
    Magic           DWORD   ?           ; 0x52584450 = 'RXDP' (RawrXD Plugin)
    Version         WORD    ?           ; 1 (never changes)
    Flags           WORD    ?           ; reserved (must be 0)
    NamePtr         QWORD   ?           ; -> ASCIIZ "MyPluginName"
    CategoryPtr     QWORD   ?           ; -> ASCIIZ "FileSystem"
    ToolCount       DWORD   ?           ; number of tools in array below
    ToolsArrayPtr   QWORD   ?           ; -> AGENT_TOOL[ToolCount]
PLUGIN_META ENDS

;==============================================================================
; AGENT TOOL DEFINITION (44 tools across all plugins)
;==============================================================================
AGENT_TOOL STRUCT
    NamePtr         QWORD   ?           ; -> "read_file"
    DescPtr         QWORD   ?           ; -> "Read file from disk"
    CategoryPtr     QWORD   ?           ; -> "FileSystem"
    VersionPtr      QWORD   ?           ; -> "1.0"
    HandlerFunc     QWORD   ?           ; -> ToolHandler(pJson)
AGENT_TOOL ENDS

;==============================================================================
; TOOL CATEGORIES (for /tools filtering)
;==============================================================================
TOOL_CAT_FILESYSTEM     equ 0
TOOL_CAT_TERMINAL       equ 1
TOOL_CAT_GIT            equ 2
TOOL_CAT_BROWSER        equ 3
TOOL_CAT_CODE           equ 4
TOOL_CAT_PROJECT        equ 5
TOOL_CAT_SYSTEM         equ 6
TOOL_CAT_PACKAGE        equ 7

;==============================================================================
; PLUGIN ENTRY POINT SIGNATURE
;==============================================================================
; Every plugin must export:
;   PluginMetaData: PLUGIN_META (static struct, never called)
;   PluginMain: void __cdecl PluginMain(void*)  (called at load)
;
; PluginMain is passed a pointer to host context (for logging, etc.)
; Return value is ignored.
;==============================================================================

;==============================================================================
; TOOL HANDLER SIGNATURE
;==============================================================================
; Every tool handler is:
;   const char* __cdecl ToolHandler(const char* pJson)
;
; Input: pJson = null-terminated JSON like {"path":"C:\\file.txt", "size": 1024}
; Output: static buffer with null-terminated JSON result like:
;         {"success":true, "data":"..."}  or
;         {"success":false, "error":"..."}
;
; Handler can use static buffers (reused across calls).
;==============================================================================

;==============================================================================
; EXPORTED SYMBOLS FROM PLUGIN
;==============================================================================
; Plugin must have:
;
;   PUBLIC PluginMetaData
;   PluginMetaData PLUGIN_META <0x52584450, 1, 0,
;       OFFSET szName, OFFSET szCategory, 3, OFFSET MyTools>
;
;   MyTools AGENT_TOOL <OFFSET szTool1Name, OFFSET szTool1Desc, ...>
;           AGENT_TOOL <OFFSET szTool2Name, OFFSET szTool2Desc, ...>
;           AGENT_TOOL <OFFSET szTool3Name, OFFSET szTool3Desc, ...>
;
; (Names/descriptions are ASCIIZ strings defined in .data)
;==============================================================================
