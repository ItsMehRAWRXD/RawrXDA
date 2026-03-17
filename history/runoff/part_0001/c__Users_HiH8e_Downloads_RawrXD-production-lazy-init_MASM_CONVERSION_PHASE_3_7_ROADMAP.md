# Qt to MASM Conversion Roadmap - Phase 3-7 Implementation Guide

**Date**: December 28, 2025  
**Focus**: Complete Pure MASM IDE Conversion  
**Target**: Feature parity with C++ Qt implementation  

---

## PHASE 3: WINDOWS UI FRAMEWORK IMPLEMENTATION (Weeks 1-3)

### Objective
Establish core Windows Common Controls (comctl32) integration in MASM for dialog boxes, tabs, lists, and other essential UI components.

---

## 3.1 DIALOG SYSTEM FRAMEWORK

### 3.1.1 Modal Dialog Routing
**File to Create**: `dialog_system.asm`  
**Size Estimate**: 800 MASM LOC  
**Dependencies**: Windows.h structures, user32.lib, comctl32.lib

**Key Functions**:
```asm
; Initialize dialog system
DialogSystemInit()           ; Setup dialog registry, message routing

; Modal dialog creation/management
CreateModalDialog()          ; Create and run modal dialog loop
CreateModelessDialog()       ; Create floating dialog
DialogMessageLoop()          ; Handle dialog-specific messages
RegisterDialogClass()        ; Register dialog window class
UnregisterDialogClass()      ; Cleanup

; Message box wrappers
MessageBoxEx()               ; Multi-option message box
ShowInfoDialog()             ; Info-only message box
ShowErrorDialog()            ; Error with OK button
ShowConfirmDialog()          ; Confirm Yes/No dialog
ShowWarningDialog()          ; Warning with OK/Cancel
```

**Implementation Strategy**:
- Create DIALOG struct to track active dialogs (handle, parent, callback, result)
- Implement dialog message pump separate from main message loop
- Handle dialog-specific messages (WM_INITDIALOG, WM_COMMAND, WM_CLOSE)
- Implement dialog result propagation via return value

**MASM Stub Pattern**:
```asm
DIALOG STRUCT
    hwnd            QWORD ?     ; Dialog window handle
    parent_hwnd     QWORD ?     ; Parent window handle
    on_command      QWORD ?     ; Command callback (function pointer)
    on_init         QWORD ?     ; Init callback
    user_data       QWORD ?     ; Custom data pointer
    result          DWORD ?     ; Dialog result (OK/Cancel/etc)
    is_modal        BYTE ?      ; Modal vs modeless
    padding         BYTE 7 DUP (?)
DIALOG ENDS
```

### 3.1.2 Common Dialog Wrappers
**File to Create**: `common_dialogs.asm`  
**Size Estimate**: 600 MASM LOC  
**Uses**: comctl32 common dialog DLL

**Key Functions**:
```asm
; File operations
OpenFileDialog()             ; GetOpenFileName wrapper
SaveFileDialog()             ; GetSaveFileName wrapper
ChooseColorDialog()          ; ChooseColorA wrapper
ChooseFontDialog()           ; ChooseFontA wrapper
BrowseForFolderDialog()      ; Folder selection
```

**Example Implementation** (FileOpen):
```asm
OpenFileDialog PROC
    LOCAL openfilename:OPENFILENAMEA
    
    ; Initialize structure
    lea rax, [openfilename]
    mov [openfilename].lStructSize, sizeof(OPENFILENAMEA)
    mov [openfilename].hwndOwner, rcx  ; Parent hwnd in rcx
    ; ... initialize other fields
    
    ; Call API
    call GetOpenFileNameA
    
    ; Return filename in rax, success/failure in result code
    ret
OpenFileDialog ENDP
```

---

## 3.2 WINDOWS COMMON CONTROLS INTEGRATION

### 3.2.1 Tab Control System
**File to Create**: `tab_control.asm`  
**Size Estimate**: 1,000 MASM LOC  
**Critical Component**: Needed by settings_dialog, multi-tab UI

**Key Structures**:
```asm
TAB_PAGE STRUCT
    title           QWORD ?     ; Page title string
    title_len       DWORD ?     ; Title length
    hwnd            QWORD ?     ; Page content window
    content_area    RECT ?      ; Content rect
TAB_PAGE ENDS

TAB_CONTROL STRUCT
    hwnd            QWORD ?     ; Tab control window
    pages           QWORD ?     ; Array of TAB_PAGE*
    page_count      DWORD ?     ; Number of pages
    active_page     DWORD ?     ; Currently active page
TAB_CONTROL ENDS
```

**Key Functions**:
```asm
CreateTabControl()           ; Create tab control window
AddTabPage()                 ; Add page to tab control
RemoveTabPage()              ; Remove page
GetActiveTabPage()           ; Get current page
SetActiveTabPage()           ; Switch to page
DestroyTabControl()          ; Cleanup
OnTabSelectionChanged()      ; Handle TCN_SELCHANGE
```

**Implementation Strategy**:
1. Use WC_TABCONTROL window class from comctl32
2. Send TCM_INSERTITEM to add pages
3. Handle TCN_SELCHANGE notifications
4. Show/hide child windows based on active page
5. Maintain page list with content area management

### 3.2.2 List View Control
**File to Create**: `listview_control.asm`  
**Size Estimate**: 1,200 MASM LOC  
**Usage**: File browser, model list, chat history

**Key Structures**:
```asm
LISTVIEW_ITEM STRUCT
    text            QWORD ?     ; Item text
    icon            DWORD ?     ; Icon index
    user_data       QWORD ?     ; Custom data
LISTVIEW_ITEM ENDS

LISTVIEW_COLUMN STRUCT
    title           QWORD ?     ; Column header
    width           DWORD ?     ; Column width in pixels
    format          DWORD ?     ; LVCFMT_LEFT, LVCFMT_RIGHT, etc.
LISTVIEW_COLUMN ENDS
```

**Key Functions**:
```asm
CreateListView()             ; WC_LISTVIEW window
AddColumn()                  ; LVM_INSERTCOLUMN
AddItem()                    ; LVM_INSERTITEM
SetItemText()                ; LVM_SETITEMTEXT
GetSelectedItem()            ; Get current selection
SetListViewStyle()           ; Details/Small icons/etc.
OnListViewSelectionChanged() ; LVN_ITEMCHANGED handler
```

### 3.2.3 Tree View Control (for File Browser)
**File to Create**: `treeview_control.asm`  
**Size Estimate**: 1,000 MASM LOC  
**Critical**: File explorer, project tree

**Key Structures**:
```asm
TREEVIEW_NODE STRUCT
    text            QWORD ?     ; Node text
    icon            DWORD ?     ; Icon index
    children        QWORD ?     ; Child nodes array
    child_count     DWORD ?     ; Number of children
    parent          QWORD ?     ; Parent node pointer
    is_expanded     BYTE ?      ; Expansion state
TREEVIEW_NODE ENDS
```

**Key Functions**:
```asm
CreateTreeView()             ; WC_TREEVIEW window
InsertNode()                 ; TVM_INSERTITEM
ExpandNode()                 ; Expand/collapse
RemoveNode()                 ; TVM_DELETEITEM
GetSelectedNode()            ; TVM_GETNEXTITEM
RefreshTree()                ; Redraw tree
OnTreeSelectionChanged()     ; TVN_SELCHANGED handler
```

### 3.2.4 Other Essential Controls
**File to Create**: `common_controls.asm`  
**Size Estimate**: 800 MASM LOC

**Controls to Wrap**:
```asm
CreateSliderControl()        ; Slider/TrackBar (TRACKBAR_CLASS)
CreateSpinnerControl()       ; Up/Down spinner (UPDOWN_CLASS)
CreateProgressBar()          ; Progress control
CreateComboBox()             ; Dropdown list (COMBOBOX_CLASS)
CreateEditControl()          ; Text input (EDIT class)
CreateButtonControl()        ; Push button (BUTTON class)
CreateCheckbox()             ; Checkbox (BUTTON class, BS_CHECKBOX)
CreateRadioButton()          ; Radio button (BUTTON class, BS_RADIOBUTTON)
CreateStaticText()           ; Static text label (STATIC class)
CreateGroupBox()             ; Grouping box (BUTTON class, BS_GROUPBOX)
```

---

## PHASE 3.3: THEME SYSTEM CONVERSION

### 3.3.1 ThemedCodeEditor Port
**File to Create**: `qt6_themed_editor.asm`  
**Size Estimate**: 1,500 MASM LOC  
**Based on**: ThemedCodeEditor.cpp (790 lines)

**Key Components**:
```asm
; Color management
GetThemeColor()              ; Get color for token type
SetThemeColorPalette()       ; Apply theme colors
GetHighlightColor()          ; Syntax highlight color

; Painting
PaintLineNumbers()           ; Render line number gutter
PaintSyntaxHighlights()      ; Color code tokens
PaintCursor()                ; Draw text cursor
PaintSelection()             ; Draw selection rectangle

; Editing
InsertText()                 ; Insert text at cursor
DeleteText()                 ; Delete selection/char
ApplySyntaxHighlight()       ; Tokenize and color
```

**Syntax Support** (Tokenize + Color):
- C++ (keywords, strings, comments, operators)
- Python (indentation-aware)
- JavaScript/TypeScript
- JSON
- XML/HTML
- Markdown

**Implementation Strategy**:
1. Implement lexer state machine for token classification
2. Use color constants from OBJECT_BASE color palette
3. Cache highlighted ranges to avoid repainting
4. Implement viewport-based rendering (only visible lines)
5. Support incremental re-highlighting on text changes

### 3.3.2 ThemeManager Port
**File to Create**: `qt6_theme_manager.asm`  
**Size Estimate**: 1,000 MASM LOC  
**Based on**: ThemeManager.cpp (1,200 lines)

**Color Palette Struct**:
```asm
COLOR_PALETTE STRUCT
    bg_primary      DWORD ?     ; Main background color (RGB)
    bg_secondary    DWORD ?     ; Secondary background
    fg_primary      DWORD ?     ; Main text color
    fg_secondary    DWORD ?     ; Secondary text
    
    keyword_color   DWORD ?     ; C++ keywords (blue)
    string_color    DWORD ?     ; String literals (green)
    comment_color   DWORD ?     ; Comments (gray)
    operator_color  DWORD ?     ; Operators (white/cyan)
    
    error_color     DWORD ?     ; Syntax errors (red)
    warning_color   DWORD ?     ; Warnings (orange)
    info_color      DWORD ?     ; Info highlights (blue)
    success_color   DWORD ?     ; Success indicators (green)
    
    font_name       QWORD ?     ; Font family name
    font_size       DWORD ?     ; Font size in points
COLOR_PALETTE ENDS
```

**Key Functions**:
```asm
CreateThemePalette()         ; Create color palette
ApplyTheme()                 ; Apply palette to all components
SetLightTheme()              ; Load light color scheme
SetDarkTheme()               ; Load dark color scheme
GetThemeColor()              ; Retrieve specific color
SaveThemeToRegistry()        ; Persist theme settings
LoadThemeFromRegistry()       ; Restore saved theme
```

**Themes to Implement**:
- **Light Theme**: Light gray backgrounds, dark text, muted colors
- **Dark Theme**: Dark backgrounds, light text, saturated colors
- **High Contrast**: Black/white, maximum readability
- **Solarized**: Popular editor color scheme

---

## PHASE 4: SETTINGS & DATA PERSISTENCE (Weeks 4-5)

### 4.1 Settings Dialog Conversion
**File to Create**: `qt6_settings_dialog.asm`  
**Size Estimate**: 2,500 MASM LOC  
**Based on**: settings_dialog.cpp (23,338 bytes - 7+ tabs)

**Settings Tabs to Implement**:
1. **General Tab**
   - Auto-save settings
   - Startup behavior
   - File associations
   
2. **Model Tab**
   - Model path selection
   - Model list (listview)
   - Default model
   
3. **AI Chat Tab**
   - Chat model selection
   - Temperature slider
   - Max tokens spinner
   - System prompt textarea
   
4. **Security Tab**
   - API key management
   - Encryption toggle
   - Secure storage checkbox
   
5. **Training Tab** (optional)
   - Training paths
   - Checkpoint intervals
   - Batch size settings
   
6. **CI/CD Tab** (optional)
   - Pipeline settings
   - GitHub integration
   
7. **Enterprise Tab** (optional)
   - Compliance logging
   - Telemetry settings

**Data Structure**:
```asm
SETTINGS_DATA STRUCT
    auto_save_enabled   BYTE ?
    startup_fullscreen  BYTE ?
    
    model_path          QWORD ?
    model_name          QWORD ?
    
    chat_model          QWORD ?
    temperature         FLOAT ?
    max_tokens          DWORD ?
    system_prompt       QWORD ?
    
    api_key             QWORD ?     ; Encrypted
    encryption_enabled  BYTE ?
    
    ; ... more fields
SETTINGS_DATA ENDS
```

**Implementation Strategy**:
1. Create tab control with 7 pages
2. Populate each tab with appropriate controls
3. Implement settings serialization (Registry + JSON)
4. Handle OK/Cancel/Apply buttons
5. Validate input on apply

### 4.2 JSON & Registry Persistence
**File to Create**: `persistence_layer.asm`  
**Size Estimate**: 1,500 MASM LOC

**Key Functions**:
```asm
; JSON support
ParseJSON()                  ; Parse JSON string
SerializeToJSON()            ; Convert struct to JSON
LoadJSONFile()               ; Read JSON from disk
SaveJSONFile()               ; Write JSON to disk

; Registry support
OpenRegistryKey()            ; RegOpenKeyEx wrapper
ReadRegistryValue()          ; RegQueryValueEx wrapper
WriteRegistryValue()         ; RegSetValueEx wrapper
DeleteRegistryKey()          ; RegDeleteKey wrapper

; Settings file operations
LoadSettings()               ; Load from registry/JSON
SaveSettings()               ; Persist to disk
ResetSettingsToDefault()     ; Restore defaults
```

**Registry Path** (for RawrXD):
```
HKEY_CURRENT_USER\Software\RawrXD\Settings
HKEY_CURRENT_USER\Software\RawrXD\Themes
HKEY_CURRENT_USER\Software\RawrXD\Models
```

### 4.3 File Browser Implementation
**File to Create**: `qt6_file_browser.asm`  
**Size Estimate**: 1,500 MASM LOC

**Core Features**:
- Tree view of directory structure
- File filtering (*.gguf, *.py, etc.)
- Right-click context menu
- File operations (open, delete, rename)
- Search/filter functionality

**Data Structure**:
```asm
FILE_ITEM STRUCT
    path            QWORD ?     ; Full file path
    filename        QWORD ?     ; Just filename
    file_size       QWORD ?     ; File size in bytes
    modified_time   QWORD ?     ; Last modified timestamp
    is_directory    BYTE ?      ; Is directory flag
    icon_index      DWORD ?     ; Icon ID (file vs folder)
FILE_ITEM ENDS

FILE_BROWSER STRUCT
    root_hwnd       QWORD ?     ; Treeview control hwnd
    current_path    QWORD ?     ; Current directory
    file_list       QWORD ?     ; Array of FILE_ITEM*
    file_count      DWORD ?     ; Number of files
FILE_BROWSER ENDS
```

**Key Functions**:
```asm
CreateFileBrowser()          ; Initialize file browser
RefreshDirectory()           ; Reload file list
FilterFiles()                ; Apply file filter
GetSelectedFile()            ; Get selected file path
OpenFile()                   ; Execute file (ShellExecute)
DeleteFile()                 ; Delete file
RenameFile()                 ; Rename file
OpenFileDialog()             ; Open file selector
```

### 4.4 Chat Session Storage
**File to Create**: `chat_persistence.asm`  
**Size Estimate**: 1,000 MASM LOC

**Chat Message Structure**:
```asm
CHAT_MESSAGE STRUCT
    sender          QWORD ?     ; "user", "assistant", "system"
    content         QWORD ?     ; Message text
    timestamp       QWORD ?     ; Unix timestamp
    model_name      QWORD ?     ; Which model generated it
    tokens_used     DWORD ?     ; Token count
CHAT_MESSAGE ENDS

CHAT_SESSION STRUCT
    session_id      QWORD ?     ; Unique session ID
    created_at      QWORD ?     ; Session creation time
    messages        QWORD ?     ; Array of CHAT_MESSAGE*
    message_count   DWORD ?     ; Number of messages
    metadata        QWORD ?     ; JSON metadata (model, etc)
CHAT_SESSION ENDS
```

**Key Functions**:
```asm
CreateChatSession()          ; Start new conversation
AddChatMessage()             ; Append message to session
SaveChatSession()            ; Write session to disk
LoadChatSession()            ; Load from JSON file
ExportChatAsMarkdown()       ; Export as .md file
ClearChatHistory()           ; Delete all messages
ListSavedSessions()          ; Enumerate saved chats
```

**Storage Location**:
```
%APPDATA%\RawrXD\Chat\
  session_001.json
  session_002.json
  ...
```

---

## PHASE 5: ADVANCED FEATURES & AI INTEGRATION (Weeks 6-9)

### 5.1 AI Chat Panel Conversion
**File to Create**: `qt6_ai_chat_panel.asm`  
**Size Estimate**: 4,000+ MASM LOC  
**Based on**: ai_chat_panel.cpp (50,504 bytes - LARGEST UI COMPONENT)

**Critical Features**:
1. **Message Display**
   - User message bubbles (right-aligned)
   - Assistant message bubbles (left-aligned)
   - Syntax-highlighted code blocks
   - Formatted text (bold, italic, links)
   
2. **Streaming Support**
   - Incremental text display
   - Token-by-token updates
   - Animation/smooth scrolling
   
3. **Context Awareness**
   - File context insertion
   - Selection highlighting
   - Breadcrumb navigation
   
4. **Input Panel**
   - Multi-line text input
   - Model selector dropdown
   - Send button
   - Clear history button

**Message Bubble Struct**:
```asm
MESSAGE_BUBBLE STRUCT
    rect            RECT ?      ; Display rectangle
    text            QWORD ?     ; Message text
    text_len        DWORD ?     ; Text length
    sender          BYTE ?      ; 0=user, 1=assistant
    timestamp       QWORD ?     ; When sent
    is_streaming    BYTE ?      ; Still being received
    visible_chars   DWORD ?     ; For streaming display
    code_blocks     QWORD ?     ; Array of code block rects
    code_count      DWORD ?     ; Number of code blocks
MESSAGE_BUBBLE ENDS
```

**Implementation Strategy**:
1. Use rich text rendering (GDI+ or custom text layout)
2. Implement word wrapping for long messages
3. Parse markdown for code blocks and formatting
4. Support syntax highlighting within code blocks
5. Implement smooth scrolling for streaming text
6. Cache rendered messages for performance

**Key Functions**:
```asm
CreateChatPanel()            ; Initialize chat UI
AddChatMessage()             ; Add message bubble
StreamingTextDisplay()       ; Show text incrementally
ParseMarkdown()              ; Extract formatting
RenderMessageBubble()        ; Paint message
OnSendButton()               ; Handle send click
ClearChat()                  ; Clear message history
ExportChat()                 ; Export conversation
```

### 5.2 Agentic Mode Handlers
**File to Create**: `qt6_agent_modes.asm`  
**Size Estimate**: 2,000 MASM LOC  
**Modes**: Plan, Ask, Agent, Architect

**Agent Mode Enum**:
```asm
AGENT_MODE_PLAN       EQU 1    ; Step-by-step planning
AGENT_MODE_ASK        EQU 2    ; Interactive questions
AGENT_MODE_AGENT      EQU 3    ; Autonomous execution
AGENT_MODE_ARCHITECT  EQU 4    ; Design generation
```

**Mode Handler Structure**:
```asm
AGENT_MODE_HANDLER STRUCT
    mode_id         DWORD ?     ; AGENT_MODE_* enum
    mode_name       QWORD ?     ; Display name
    description     QWORD ?     ; Mode description
    system_prompt   QWORD ?     ; System prompt for this mode
    on_activate     QWORD ?     ; Callback on mode switch
    on_deactivate   QWORD ?     ; Callback on mode change
    settings        QWORD ?     ; Mode-specific settings
AGENT_MODE_HANDLER ENDS
```

**Key Functions**:
```asm
SetAgentMode()               ; Switch to mode
GetCurrentAgentMode()        ; Get active mode
GetModeSystemPrompt()        ; Get prompt for mode
ApplyModeFormatting()        ; Format response per mode
```

**Mode Implementations**:
- **PLAN**: Shows step-by-step reasoning
- **ASK**: Interactive clarifying questions
- **AGENT**: Autonomous tool execution
- **ARCHITECT**: System design generation

### 5.3 Agentic Tool System
**File to Create**: `qt6_agentic_tools.asm`  
**Size Estimate**: 1,500 MASM LOC

**Tool Registry**:
```asm
TOOL_DEFINITION STRUCT
    tool_name       QWORD ?     ; Tool identifier
    description     QWORD ?     ; Human description
    parameters      QWORD ?     ; JSON schema of parameters
    execute_func    QWORD ?     ; Function pointer to executor
    availability    BYTE ?      ; Whether tool is available
TOOL_DEFINITION ENDS

TOOL_REGISTRY STRUCT
    tools           QWORD ?     ; Array of TOOL_DEFINITION*
    tool_count      DWORD ?     ; Number of registered tools
TOOL_REGISTRY ENDS
```

**Available Tools**:
```asm
; File operations
file_read_tool()             ; Read file contents
file_write_tool()            ; Write to file
file_list_tool()             ; List directory

; Code analysis
analyze_code_tool()          ; AST analysis
find_errors_tool()           ; Syntax checking
refactor_code_tool()         ; Code transformation

; Model operations
run_model_tool()             ; Execute inference
list_models_tool()           ; Show available models
load_model_tool()            ; Load model

; Development
run_command_tool()           ; Execute shell command
git_command_tool()           ; Git operations
search_files_tool()          ; File search
```

**Key Functions**:
```asm
RegisterTool()               ; Add tool to registry
UnregisterTool()             ; Remove tool
GetAvailableTools()          ; List usable tools
ExecuteTool()                ; Run tool with params
```

---

## PHASE 6: GPU & OPTIMIZATION INTEGRATION (Weeks 10-14)

### 6.1 Network Integration - HTTP Client
**File to Create**: `http_client.asm`  
**Size Estimate**: 2,500 MASM LOC  
**Needed by**: Ollama API, model downloading, inference servers

**HTTP Operations**:
```asm
CreateHttpConnection()       ; Create HTTP socket
SendHttpRequest()            ; Send HTTP request
ReceiveHttpResponse()        ; Read HTTP response
CloseHttpConnection()        ; Close connection

; Convenience functions
HttpGet()                    ; GET request
HttpPost()                   ; POST request
HttpPostJSON()               ; POST with JSON body
HttpGetFile()                ; Download file
```

**HTTP Request Structure**:
```asm
HTTP_REQUEST STRUCT
    method          QWORD ?     ; "GET", "POST", "PUT"
    url             QWORD ?     ; Full URL
    headers         QWORD ?     ; Header key-value pairs
    header_count    DWORD ?     ; Number of headers
    body            QWORD ?     ; Request body (POST)
    body_len        DWORD ?     ; Body length
HTTP_REQUEST ENDS

HTTP_RESPONSE STRUCT
    status_code     DWORD ?     ; HTTP status (200, 404, etc)
    headers         QWORD ?     ; Response headers
    body            QWORD ?     ; Response body
    body_len        DWORD ?     ; Body length
HTTP_RESPONSE ENDS
```

**Ollama API Integration**:
```asm
OllamaListModels()           ; GET /api/tags
OllamaLoadModel()            ; POST /api/pull
OllamaGenerate()             ; POST /api/generate (streaming)
OllamaChatCompletion()       ; POST /api/chat (streaming)
```

### 6.2 GGUF Loader Enhancement
**File to Create**: `qt6_gguf_loader_enhanced.asm`  
**Size Estimate**: 2,500 MASM LOC  
**Based on**: gguf_loader.cpp (1,200 lines)

**GGUF File Format**:
```asm
GGUF_HEADER STRUCT
    magic           DWORD ?     ; 0x46554747 ("GGUF" little-endian)
    version         DWORD ?     ; Format version
    tensor_count    QWORD ?     ; Number of tensors
    metadata_size   QWORD ?     ; Metadata section size
GGUF_HEADER ENDS

GGUF_TENSOR STRUCT
    name            QWORD ?     ; Tensor name
    n_dims          DWORD ?     ; Number of dimensions
    dims            QWORD ?     ; Shape array
    data_type       DWORD ?     ; Data type enum
    offset          QWORD ?     ; Offset in file
    data_size       QWORD ?     ; Size in bytes
GGUF_TENSOR ENDS
```

**Key Functions**:
```asm
LoadGGUFFile()               ; Open and parse GGUF file
GetTensorByName()            ; Find tensor in loaded model
GetModelMetadata()           ; Extract model parameters
GetQuantizationType()        ; Detect quantization level
AllocateModelMemory()        ; Reserve RAM for model
LoadTensorData()             ; Load tensor into memory
ValidateGGUFFormat()         ; Verify file integrity
```

**Supported Quantization Levels**:
- fp32 (full precision, 4 bytes per element)
- fp16 (half precision, 2 bytes per element)
- bf16 (brain float, 2 bytes)
- q8_0 (8-bit quantized)
- q4_0 (4-bit quantized)
- q4_1 (4-bit with scale/offset)
- q5_0, q5_1 (5-bit variants)

### 6.3 GPU Backend Stubs (Optional)
**File to Create**: `gpu_backend_stubs.asm`  
**Size Estimate**: 2,000+ MASM LOC per backend  
**Backends**: CUDA, Vulkan, Metal (Metal for future macOS support)

**Note**: GPU compute is complex and can be left as optional C++ DLL for now.

#### CUDA Stub (NVIDIA)
```asm
CudaInitialize()             ; cudaInitDevice
CudaAllocate()               ; cudaMalloc
CudaDeallocate()             ; cudaFree
CudaMemcpy()                 ; cudaMemcpy (host↔device)
CudaLaunchKernel()           ; Launch compute kernel
```

#### Vulkan Stub (Cross-platform)
```asm
VulkanCreateInstance()       ; Create Vulkan instance
VulkanCreateDevice()         ; Create logical device
VulkanAllocateBuffer()       ; Allocate GPU memory
VulkanCreateComputePipeline()  ; Compile compute shader
VulkanDispatchCompute()      ; Run compute shader
```

---

## PHASE 7: TOKENIZATION & TRAINING (Weeks 15-17, Optional)

### 7.1 Tokenizer Implementation
**File to Create**: `tokenizers.asm`  
**Size Estimate**: 5,000+ MASM LOC  
**Data Tables**: 500KB+ for vocabulary/merge rules

**BPE (Byte-Pair Encoding) Tokenizer**:
```asm
BPE_VOCAB_ENTRY STRUCT
    token_id        DWORD ?     ; Token number
    text            QWORD ?     ; Token text
    priority        DWORD ?     ; Merge priority
BPE_VOCAB_ENTRY ENDS

BPETokenizer STRUCT
    vocab_table     QWORD ?     ; Hash table of tokens
    merge_rules     QWORD ?     ; Byte-pair merge list
    special_tokens  QWORD ?     ; Special token definitions
    vocab_size      DWORD ?     ; Number of tokens
BPETokenizer ENDS
```

**Key Functions**:
```asm
InitializeBPETokenizer()     ; Load vocabulary
TokenizeText()               ; Convert text → token IDs
DetokenizeTokens()           ; Convert token IDs → text
GetTokenId()                 ; Text → ID lookup
GetTokenText()               ; ID → text lookup
```

**Tokenizer Types to Implement**:
1. **BPE** (OpenAI GPT-2/GPT-3)
2. **SentencePiece** (LLaMA, Mistral)
3. **WordPiece** (BERT)

### 7.2 Training Infrastructure (Optional)
**File to Create**: `training_system.asm`  
**Size Estimate**: 3,000+ MASM LOC

**Note**: Training is optional and can remain in C++ if GPU integration is unavailable.

```asm
CreateTrainingSession()      ; Initialize trainer
LoadDataset()                ; Load training data
CreateCheckpoint()           ; Save model state
LoadCheckpoint()             ; Restore model state
ResumeTraining()             ; Continue from checkpoint
```

---

## COMPILATION & TESTING STRATEGY

### Build System
**Create**: `build_all_phases.bat`

```batch
@echo off
setlocal enabledelayedexpansion

REM Phase 3: UI Framework
echo Compiling Phase 3 UI Framework...
ml64 /c /Fo obj\ dialog_system.asm
ml64 /c /Fo obj\ common_dialogs.asm
ml64 /c /Fo obj\ tab_control.asm
ml64 /c /Fo obj\ listview_control.asm
ml64 /c /Fo obj\ qt6_themed_editor.asm

REM Phase 4: Data Persistence
echo Compiling Phase 4 Persistence...
ml64 /c /Fo obj\ qt6_settings_dialog.asm
ml64 /c /Fo obj\ persistence_layer.asm
ml64 /c /Fo obj\ qt6_file_browser.asm

REM Phase 5: AI Integration
echo Compiling Phase 5 AI Features...
ml64 /c /Fo obj\ qt6_ai_chat_panel.asm
ml64 /c /Fo obj\ qt6_agent_modes.asm
ml64 /c /Fo obj\ qt6_agentic_tools.asm

REM Link everything
echo Linking executable...
link /SUBSYSTEM:WINDOWS /ENTRY:_start obj\*.obj ...

echo Build complete!
```

### Testing Framework
**Create**: `test_framework.asm`

```asm
; Unit test harness
TestModule STRUCT
    name            QWORD ?     ; Module name
    test_count      DWORD ?     ; Number of tests
    passed_count    DWORD ?     ; Tests passed
    failed_tests    QWORD ?     ; Array of failed test IDs
TESTMODULE ENDS

; Test assertion macros
TEST_ASSERT_EQ()             ; Assert equality
TEST_ASSERT_NE()             ; Assert not equal
TEST_ASSERT_TRUE()           ; Assert true condition
TEST_ASSERT_FALSE()          ; Assert false condition
RUN_TEST_SUITE()             ; Execute all tests
PRINT_TEST_RESULTS()         ; Display results
```

---

## SUMMARY TABLE: PHASES 3-7 COMPONENTS

| Phase | Category | Files | MASM LOC | Est. Time | Priority |
|-------|----------|-------|----------|-----------|----------|
| **3** | UI Framework | 7 | 4,500 | 2-3 wks | CRITICAL |
| **4** | Data Persistence | 4 | 3,500 | 2 wks | CRITICAL |
| **5** | AI Features | 3 | 8,000 | 3-4 wks | HIGH |
| **6** | GPU/Network | 3 | 7,500+ | 4+ wks | MEDIUM |
| **7** | Tokenizers/Training | 2 | 8,000+ | 2-3 wks | LOW |
| **TOTAL** | | 22 | 31,500+ | 13-16 wks | — |

---

## SUCCESS CRITERIA

### By End of Phase 3
✅ All Windows Common Controls working in pure MASM  
✅ Dialog system functional  
✅ Tab control system operational  
✅ Theme system applied  

### By End of Phase 4
✅ Settings dialog fully functional  
✅ File browser operational  
✅ Chat persistence working  
✅ Settings saved/loaded from registry  

### By End of Phase 5
✅ AI chat panel displaying messages  
✅ Streaming text display working  
✅ Agent mode switching functional  
✅ Tool execution system operational  

### By End of Phase 6
✅ Ollama API integration complete  
✅ GGUF model loading working  
✅ GPU backends available (CUDA/Vulkan stubs)  

### By End of Phase 7
✅ Tokenization system functional  
✅ Training infrastructure operational (optional)  
✅ 100% pure MASM IDE feature parity with C++ Qt version  

---

## RESOURCE REQUIREMENTS

### Development Tools
- ✅ ml64.exe (MASM64) - Already installed
- ✅ link.exe (Microsoft Linker) - Already installed
- ✅ Windows SDK (user32.lib, kernel32.lib, comctl32.lib, gdi32.lib) - Already installed
- ⚠️ GDI+ SDK (for advanced drawing) - Optional
- ⚠️ CUDA Toolkit (for GPU) - Optional

### Knowledge Requirements
- Windows API (common controls, dialogs, registry)
- x64 MASM syntax & conventions
- Binary file formats (JSON, GGUF, registry hives)
- Networking protocols (HTTP, TCP/IP)
- GPU compute (CUDA, Vulkan) - for Phase 6

### Time Estimate
- **Experienced MASM developer**: 3-4 months (part-time)
- **Team of 3**: 1-2 months (concurrent development)
- **With external GPU library**: Can skip Phase 6 GPU code (1 month saved)

