// MASM Agentic Bridge - Expose pure MASM functions to Qt/C++
#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantMap>

// MASM function declarations (stdcall calling convention)
extern "C" {
    // IDE Master Integration (ide_master_integration.asm)
    int __stdcall IDEMaster_Initialize();
    int __stdcall IDEMaster_LoadModel(const char* modelPath, int loadMethod);
    int __stdcall IDEMaster_HotSwapModel(int newModelID);
    int __stdcall IDEMaster_ExecuteAgenticTask(const char* task);
    int __stdcall IDEMaster_SaveWorkspace(const char* path);
    int __stdcall IDEMaster_LoadWorkspace(const char* path);
    
    // Browser Agent (autonomous_browser_agent.asm)
    int __stdcall BrowserAgent_Init();
    int __stdcall BrowserAgent_Navigate(const char* url);
    const char* __stdcall BrowserAgent_GetDOM();
    const char* __stdcall BrowserAgent_ExtractText();
    int __stdcall BrowserAgent_ClickElement(const char* elementID);
    int __stdcall BrowserAgent_FillForm(const char* fieldID, const char* value);
    int __stdcall BrowserAgent_ExecuteScript(const char* script);
    
    // Model Hotpatch Engine (model_hotpatch_engine.asm)
    int __stdcall HotPatch_Init();
    int __stdcall HotPatch_RegisterModel(const char* path, const char* name, int sourceType);
    int __stdcall HotPatch_SwapModel(int modelID);
    int __stdcall HotPatch_RollbackModel();
    int __stdcall HotPatch_CacheModel(int modelID);
    int __stdcall HotPatch_WarmupModel(int modelID);
    
    // Agentic IDE Full Control (agentic_ide_full_control.asm) - 58 tools
    int __stdcall AgenticIDE_Initialize();
    int __stdcall AgenticIDE_ExecuteTool(int toolID, const char* params);
    int __stdcall AgenticIDE_ExecuteToolChain(int* toolArray, const char** paramArray, int count);
    int __stdcall AgenticIDE_SetToolEnabled(int toolID, int enabled);
    int __stdcall AgenticIDE_IsToolEnabled(int toolID);
    const char* __stdcall AgenticIDE_GetToolName(int toolID);
    const char* __stdcall AgenticIDE_GetToolDescription(int toolID);
}

// Tool IDs (58 total from agentic_ide_full_control.asm)
enum class AgenticTool {
    FileRead = 1,
    FileWrite = 2,
    FileList = 3,
    FileSearch = 4,
    CodeCompile = 5,
    CodeRun = 6,
    CodeDebug = 7,
    CodeFormat = 8,
    WebNavigate = 9,
    WebExtract = 10,
    WebClick = 11,
    WebFill = 12,
    ModelLoad = 13,
    ModelSwap = 14,
    ModelList = 15,
    ModelInfer = 16,
    ModelStreamLoad = 17,
    ModelSetCap = 18,
    ModelCompress = 19,
    ModelDequant = 20,
    ModelSelectBackend = 21,
    PaneCreate = 22,
    PaneClose = 23,
    PaneMove = 24,
    PaneRender = 25,
    ThemeSet = 26,
    LayoutSave = 27,
    LayoutLoad = 28,
    BackendSelect = 29,
    BackendList = 30,
    SearchCode = 31,
    ReplaceCode = 32,
    Refactor = 33,
    ExtractFunc = 34,
    Rename = 35,
    GitCommit = 36,
    GitPush = 37,
    GitPull = 38,
    TerminalExec = 39,
    HttpGet = 40,
    HttpPost = 41,
    CloudUpload = 42,
    CloudDownload = 43,
    Quantize = 44,
    Benchmark = 45,
    Profile = 46,
    MemoryAnalyze = 47,
    ErrorLog = 48,
    Screenshot = 49,
    WorkspaceSave = 50,
    WorkspaceLoad = 51,
    PluginLoad = 52,
    PluginUnload = 53,
    PluginList = 54,
    ConfigGet = 55,
    ConfigSet = 56,
    EventSubscribe = 57,
    EventPublish = 58
};

class MASMAgenticBridge : public QObject {
    Q_OBJECT
    
public:
    static MASMAgenticBridge* instance();
    
    // IDE Master
    bool initializeIDE();
    bool loadModel(const QString& modelPath, int loadMethod = 0);
    bool hotSwapModel(int newModelID);
    QString executeAgenticTask(const QString& task);
    bool saveWorkspace(const QString& path);
    bool loadWorkspace(const QString& path);
    
    // Browser Agent
    bool initializeBrowser();
    bool navigateTo(const QString& url);
    QString getDOMContent();
    QString extractPageText();
    bool clickElement(const QString& elementID);
    bool fillFormField(const QString& fieldID, const QString& value);
    QString executeScript(const QString& script);
    
    // Model Hotpatch
    bool initializeHotpatch();
    int registerModel(const QString& path, const QString& name, int sourceType);
    bool swapToModel(int modelID);
    bool rollbackModel();
    bool cacheModel(int modelID);
    bool warmupModel(int modelID);
    
    // Agentic Tools (58 tools)
    bool initializeTools();
    bool executeTool(int toolID, const QVariantMap& params);
    bool executeToolChain(const QList<int>& tools, const QList<QVariantMap>& params);
    bool setToolEnabled(int toolID, bool enabled);
    bool isToolEnabled(int toolID);
    QString getToolName(int toolID);
    QString getToolDescription(int toolID);
    
signals:
    void ideInitialized();
    void modelLoaded(const QString& path);
    void modelSwapped(int modelID);
    void agenticTaskComplete(const QString& result);
    void toolExecuted(int toolID, bool success);
    void browserNavigated(const QString& url);
    void errorOccurred(const QString& error);
    
public:
    explicit MASMAgenticBridge(QObject* parent = nullptr);
    ~MASMAgenticBridge();
    
private:
    bool m_ideInitialized{false};
    bool m_browserInitialized{false};
    bool m_hotpatchInitialized{false};
    bool m_toolsInitialized{false};
    int m_activeModelID{-1};
};
