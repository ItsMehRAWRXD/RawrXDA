# Bare Return Function Audit

- Scope: `src/**/*.cpp|cc|cxx|c|h|hpp`
- Criteria: function bodies with only one statement: `return 0;`, `return false;`, `return nullptr;`, `return {};`, or `return "" ;`

- Total matches: 345
- Files matched: 44

## src\agentic\agentic_command_executor.cpp
- L152: `std::string AgenticCommandExecutor::getOutput() const` -> `return {};`

## src\agentic\agentic_executor.cpp
- L179: `bool AgenticExecutor::isTrainingModel() const` -> `return false;`

## src\agentic\tests\smoke_test.cpp
- L4: `int main()` -> `return 0;`

## src\agentic_ide_new.cpp
- L447: `std::shared_ptr<T> AgenticIDE::getComponent() const` -> `return nullptr;`

## src\benchmark_menu_widget.cpp
- L399: `uint32_t BenchmarkLogOutput::levelToColor(LogLevel)` -> `return 0;`

## src\cli\enhanced_cli.cpp
- L21: `char** rl_completion_matches(const char* text, char* (*entry_func)(const char*, int))` -> `return nullptr;`
- L23: `void* rl_get_startup_hook()` -> `return nullptr;`

## src\core\js_extension_host.cpp
- L98: `static JSRuntime* JS_NewRuntime(void)` -> `return nullptr;`
- L102: `static JSContext* JS_NewContext(JSRuntime* rt)` -> `return nullptr;`
- L116: `static const char* JS_ToCString(JSContext* ctx, JSValue val)` -> `return "";`
- L118: `static int JS_ToBool(JSContext* ctx, JSValue val)` -> `return 0;`
- L123: `static int JS_IsString(JSValue val)` -> `return 0;`
- L124: `static int JS_IsObject(JSValue val)` -> `return 0;`
- L129: `static int JS_ExecutePendingJob(JSRuntime* rt, JSContext** pctx)` -> `return 0;`
- L135: `static void* JS_GetContextOpaque(JSContext* ctx)` -> `return nullptr;`
- L137: `static int JS_IsFunction(JSContext* ctx, JSValue val)` -> `return 0;`
- L140: `static int32_t JS_GetPropertyLength(JSContext* ctx, JSValue obj)` -> `return 0;`

## src\core\js_extension_host_stub.cpp
- L47: `bool JSExtensionHost::isInitialized() const` -> `return false;`
- L54: `bool JSExtensionHost::isJSExtension(const VSCodeExtensionManifest*) const` -> `return false;`
- L58: `uint64_t JSExtensionHost::createTimer(uint64_t, bool, void*)` -> `return 0;`
- L60: `JSExtensionHost::Stats JSExtensionHost::getStats() const` -> `return {};`

## src\core\link_stubs_final.cpp
- L1596: `bool GGUFLoader::Open(const std::string&)` -> `return false;`
- L1597: `bool GGUFLoader::Close()` -> `return false;`
- L1598: `bool GGUFLoader::ParseHeader()` -> `return false;`
- L1599: `bool GGUFLoader::ParseMetadata()` -> `return false;`
- L1600: `bool GGUFLoader::LoadTensorRange(uint64_t, uint64_t, std::vector<uint8_t>&)` -> `return false;`
- L2334: `bool AgenticBridge::DispatchModelToolCalls(const std::string&, std::string&)` -> `return false;`
- L2352: `AgentEvent AgentEvent::fromJSONL(const std::string&)` -> `return {};`
- L2451: `int asm_kquant_cpuid_check()` -> `return 0;`
- L2454: `const void* find_pattern_asm(const void*, size_t, const void*, size_t)` -> `return nullptr;`

## src\core\link_stubs_remaining_classes.cpp
- L1955: `bool GameEngineManager::initialize()` -> `return false;`
- L1957: `bool GameEngineManager::openProject(const std::string&)` -> `return false;`
- L1958: `bool GameEngineManager::closeProject()` -> `return false;`
- L1959: `bool GameEngineManager::isProjectOpen() const` -> `return false;`
- L1961: `std::string GameEngineManager::getActiveProjectName() const` -> `return "";`
- L1963: `bool GameEngineManager::enterPlayMode()` -> `return false;`
- L1964: `bool GameEngineManager::exitPlayMode()` -> `return false;`
- L1965: `bool GameEngineManager::pausePlayMode()` -> `return false;`
- L1966: `bool GameEngineManager::resumePlayMode()` -> `return false;`
- L1967: `bool GameEngineManager::isInPlayMode() const` -> `return false;`
- L1968: `bool GameEngineManager::startProfiler()` -> `return false;`
- L1969: `bool GameEngineManager::stopProfiler()` -> `return false;`
- L1970: `bool GameEngineManager::isProfilerRunning() const` -> `return false;`
- L1971: `GameProfilerSnapshotUnified GameEngineManager::getProfilerSnapshot() const` -> `return {};`
- L1972: `bool GameEngineManager::startDebugSession()` -> `return false;`
- L1973: `bool GameEngineManager::stopDebugSession()` -> `return false;`
- L1974: `bool GameEngineManager::compile()` -> `return false;`
- L1975: `std::vector<std::string> GameEngineManager::getCompileErrorStrings() const` -> `return {};`
- L1976: `std::string GameEngineManager::generateAIProjectSummary() const` -> `return "";`
- L1977: `std::string GameEngineManager::generateAISceneDescription() const` -> `return "";`
- L1978: `std::vector<GameEngineManager::EngineInstallation> GameEngineManager::getAllInstallations() const` -> `return {};`
- L1979: `std::string GameEngineManager::getStatusString() const` -> `return "";`
- L1980: `std::string GameEngineManager::getHelpText() const` -> `return "";`
- L1998: `bool UnrealEngineIntegration::createPlugin(const std::string&, const std::string&)` -> `return false;`
- L1999: `std::vector<std::string> UnrealEngineIntegration::getBlueprints(const std::string&) const` -> `return {};`
- L2000: `bool UnrealEngineIntegration::isPIEPaused() const` -> `return false;`
- L2001: `bool UnrealEngineIntegration::packageProject(UnrealBuildTarget, const std::string&)` -> `return false;`
- L2002: `bool UnrealEngineIntegration::cookContent(UnrealBuildTarget)` -> `return false;`
- L2003: `bool UnrealEngineIntegration::isLiveCodingEnabled() const` -> `return false;`
- L2004: `bool UnrealEngineIntegration::disableLiveCoding()` -> `return false;`
- L2005: `bool UnrealEngineIntegration::enableLiveCoding()` -> `return false;`
- L2006: `bool UnrealEngineIntegration::createCppClass(const std::string&, const std::string&, const std::string&)` -> `return false;`
- L2007: `std::vector<UnrealLevelInfo> UnrealEngineIntegration::getLevels() const` -> `return {};`
- L2008: `bool UnrealEngineIntegration::generateProjectFiles()` -> `return false;`
- L2009: `bool UnrealEngineIntegration::createModule(const std::string&, const std::string&)` -> `return false;`
- L2021: `std::vector<std::string> UnityEngineIntegration::getInstalledPackages() const` -> `return {};`
- L2022: `bool UnityEngineIntegration::isPaused() const` -> `return false;`
- L2023: `std::string UnityEngineIntegration::generateScriptTemplate(const std::string&, const std::string&, const std::string&) const` -> `return "";`
- L2024: `std::vector<UnityAssetInfo> UnityEngineIntegration::getAssets(const std::string&) const` -> `return {};`
- L2025: `std::vector<UnitySceneInfo> UnityEngineIntegration::getScenes() const` -> `return {};`
- L2026: `UnityProjectInfo UnityEngineIntegration::getProjectInfo() const` -> `return {};`
- L2071: `std::string RawrXDLSPServer::pollOutgoing()` -> `return "";`
- L2073: `uint64_t RawrXDLSPServer::getIndexedSymbolCount() const` -> `return 0;`
- L2074: `uint64_t RawrXDLSPServer::getTrackedFileCount() const` -> `return 0;`
- L2076: `ServerStats RawrXDLSPServer::getStats() const` -> `return {};`
- L2077: `std::string RawrXDLSPServer::getStatsString() const` -> `return "";`
- L2111: `std::vector<ToolDefinition> MCPServer::listTools() const` -> `return {};`
- L2113: `std::vector<ResourceDefinition> MCPServer::listResources() const` -> `return {};`
- L2115: `std::vector<PromptTemplate> MCPServer::listPrompts() const` -> `return {};`
- L2141: `bool Win32PluginLoader::loadPlugin(const std::string&)` -> `return false;`
- L2142: `bool Win32PluginLoader::unloadPlugin(const std::string&)` -> `return false;`
- L2144: `uint64_t Win32PluginLoader::pluginCount() const` -> `return 0;`
- L2145: `std::vector<std::string> Win32PluginLoader::pluginNames() const` -> `return {};`
- L2146: `const PluginInstance* Win32PluginLoader::getPlugin(const std::string&) const` -> `return nullptr;`
- L2147: `bool Win32PluginLoader::isLoaded(const std::string&) const` -> `return false;`
- L2148: `std::vector<std::string> Win32PluginLoader::scanDirectory(const std::string&) const` -> `return {};`
- L2167: `bool VSIXLoader::LoadPlugin(const std::string&)` -> `return false;`
- L2168: `bool VSIXLoader::UnloadPlugin(const std::string&)` -> `return false;`
- L2169: `bool VSIXLoader::ReloadPlugin(const std::string&)` -> `return false;`
- L2170: `std::vector<VSIXPlugin*> VSIXLoader::GetLoadedPlugins()` -> `return {};`
- L2171: `VSIXPlugin* VSIXLoader::GetPlugin(const std::string&)` -> `return nullptr;`

## src\core\memory_patch_byte_search_stubs.cpp
- L64: `int asm_detect_memory_conflicts(void*, size_t)` -> `return 0;`
- L66: `int asm_validate_patch_integrity(void*, size_t, uint32_t)` -> `return 0;`
- L67: `int asm_optimize_cache_locality(void*, size_t)` -> `return 0;`
- L68: `uint64_t asm_monitor_patch_performance(void*, size_t, uint32_t*)` -> `return 0;`
- L69: `int asm_autonomous_memory_heal(void*, size_t, uint32_t)` -> `return 0;`
- L125: `uint32_t asm_crypto_verify_patch(const void*, size_t, uint32_t, uint32_t)` -> `return 0;`
- L127: `int asm_detect_byte_conflicts(const void*, size_t, const void*, size_t)` -> `return 0;`
- L128: `int asm_ml_optimize_patch_order(const void**, size_t*, size_t, uint32_t*)` -> `return 0;`

## src\core\mesh_brain_asm_stubs.cpp
- L15: `uint32_t asm_mesh_topology_count(void)` -> `return 0;`

## src\core\RichEditEditorEngine.cpp
- L501: `bool RichEditEditorEngine::onKeyDown(WPARAM, LPARAM)` -> `return false;`
- L502: `bool RichEditEditorEngine::onChar(WCHAR)` -> `return false;`
- L503: `bool RichEditEditorEngine::onMouseWheel(int, int, int)` -> `return false;`
- L504: `bool RichEditEditorEngine::onLButtonDown(int, int, WPARAM)` -> `return false;`
- L505: `bool RichEditEditorEngine::onLButtonUp(int, int)` -> `return false;`
- L506: `bool RichEditEditorEngine::onMouseMove(int, int, WPARAM)` -> `return false;`
- L507: `bool RichEditEditorEngine::onIMEComposition(HWND, WPARAM, LPARAM)` -> `return false;`

## src\core\sqlite3.c
- L26812: `** sqlite3_config() before SQLite will operate. */ /* #include "sqliteInt.h" */ /* ** This version of the memory allocator is the default. It is ** used when no other memory allocator is specified using compile-time ** macros. */ #ifdef SQLITE_ZERO_MALLOC /* ** No-op versions of all memory allocation routines */ static void *sqlite3MemMalloc(int nByte)` -> `return 0;`
- L26829: `static int sqlite3MemSize(void *pPrior)` -> `return 0;`
- L38298: `static int kvvfsDeviceCharacteristics(sqlite3_file *pProtoFile)` -> `return 0;`

## src\core\stubs.cpp
- L147: `bool poll_event(HotpatchEvent*)` -> `return false;`
- L309: `bool AgenticBridge::DispatchModelToolCalls(const std::string&, std::string&)` -> `return false;`
- L315: `std::string DiagnosticUtils::ReadDigestFile(const std::wstring&)` -> `return "";`
- L316: `bool DiagnosticUtils::VerifyDigestFileCreated(const std::wstring&)` -> `return false;`
- L317: `bool DiagnosticUtils::OpenFileInEditor(HWND*, const std::wstring&)` -> `return false;`
- L318: `bool DiagnosticUtils::SendHotkey(HWND*, unsigned int, bool, bool, bool)` -> `return false;`
- L319: `HWND* DiagnosticUtils::FindIDEMainWindow(unsigned long)` -> `return nullptr;`
- L320: `unsigned long DiagnosticUtils::LaunchIDEProcess(const std::wstring&)` -> `return 0;`
- L324: `std::string IDEDiagnosticAutoHealer::GenerateDiagnosticReport()` -> `return "";`
- L331: `long IDEDiagnosticAutoHealer::ExecuteIDELaunch()` -> `return 0;`
- L341: `std::vector<RawrXD::TensorRef> RawrXD::StreamingGGUFLoader::GetTensorIndex() const` -> `return {};`
- L346: `bool GGUFLoader::Open(const std::string&)` -> `return false;`
- L347: `bool GGUFLoader::Close()` -> `return false;`
- L348: `bool GGUFLoader::ParseHeader()` -> `return false;`
- L349: `bool GGUFLoader::ParseMetadata()` -> `return false;`
- L355: `bool RawrXD::CPUInferenceEngine::isModelLoaded() const` -> `return false;`
- L356: `std::vector<int> RawrXD::CPUInferenceEngine::Tokenize(const std::string&)` -> `return {};`
- L385: `unsigned int RawrXD::Debugger::NativeDebuggerEngine::addWatch(const std::string&)` -> `return 0;`

## src\core\universal_stub.cpp
- L30: `int sqlite3_close(sqlite3*)` -> `return 0;`
- L35: `int sqlite3_finalize(sqlite3_stmt*)` -> `return 0;`
- L36: `int sqlite3_reset(sqlite3_stmt*)` -> `return 0;`
- L37: `int sqlite3_clear_bindings(sqlite3_stmt*)` -> `return 0;`
- L38: `int sqlite3_bind_int(sqlite3_stmt*, int, int)` -> `return 0;`
- L39: `int sqlite3_bind_int64(sqlite3_stmt*, int, int64_t)` -> `return 0;`
- L40: `int sqlite3_bind_double(sqlite3_stmt*, int, double)` -> `return 0;`
- L41: `int sqlite3_bind_text(sqlite3_stmt*, int, const char*, int, void(*)(void*))` -> `return 0;`
- L42: `int sqlite3_bind_blob(sqlite3_stmt*, int, const void*, int, void(*)(void*))` -> `return 0;`
- L43: `int sqlite3_bind_null(sqlite3_stmt*, int)` -> `return 0;`
- L44: `int sqlite3_column_count(sqlite3_stmt*)` -> `return 0;`
- L45: `const char* sqlite3_column_name(sqlite3_stmt*, int)` -> `return "";`
- L47: `int64_t sqlite3_last_insert_rowid(sqlite3*)` -> `return 0;`
- L48: `int sqlite3_changes(sqlite3*)` -> `return 0;`
- L49: `int sqlite3_total_changes(sqlite3*)` -> `return 0;`
- L68: `bool addNode(const SwarmNodeInfo&)` -> `return false;`
- L70: `bool removeNode(unsigned int)` -> `return false;`
- L71: `bool getNode(unsigned int, SwarmNodeInfo&) const` -> `return false;`
- L72: `unsigned int getOnlineNodeCount() const` -> `return 0;`
- L74: `bool collectResults(void*)` -> `return false;`
- L110: `bool UnifiedHotpatchManager_poll_event(void*)` -> `return false;`
- L119: `int asm_kquant_cpuid_check()` -> `return 0;`
- L126: `const void* find_pattern_asm(const void*, size_t, const void*, size_t)` -> `return nullptr;`
- L132: `bool LSP_Initialize()` -> `return false;`
- L134: `std::string LSP_GetDiagnostics()` -> `return "";`
- L135: `std::string LSP_GetCompletions(const std::string&, int, int)` -> `return "";`
- L136: `bool LSP_GotoDefinition(const std::string&, int, int)` -> `return false;`
- L158: `int AgenticMode_Execute(const char*)` -> `return 0;`
- L159: `bool AgenticMode_IsEnabled()` -> `return false;`
- L167: `int GapFuzzMode_Stop()` -> `return 0;`
- L168: `bool GapFuzzMode_IsRunning()` -> `return false;`
- L174: `int CompileMode_Clean()` -> `return 0;`
- L187: `bool DiskRecovery_IsSupported()` -> `return false;`

## src\core\WebView2EditorEngine.cpp
- L594: `bool WebView2EditorEngine::onKeyDown(WPARAM, LPARAM)` -> `return false;`
- L595: `bool WebView2EditorEngine::onChar(WCHAR)` -> `return false;`
- L596: `bool WebView2EditorEngine::onMouseWheel(int, int, int)` -> `return false;`
- L597: `bool WebView2EditorEngine::onLButtonDown(int, int, WPARAM)` -> `return false;`
- L598: `bool WebView2EditorEngine::onLButtonUp(int, int)` -> `return false;`
- L599: `bool WebView2EditorEngine::onMouseMove(int, int, WPARAM)` -> `return false;`
- L600: `bool WebView2EditorEngine::onIMEComposition(HWND, WPARAM, LPARAM)` -> `return false;`

## src\enhanced_cli.cpp
- L30: `char** rl_completion_matches(const char*, char* (*)(const char*, int))` -> `return nullptr;`

## src\ggml-backend-reg.cpp
- L141: `static const char * dl_error()` -> `return "";`

## src\ggml-hexagon\ggml-hexagon.cpp
- L1929: `case GGML_TYPE_Q4_0: case GGML_TYPE_Q8_0: case GGML_TYPE_MXFP4: if (src0->ne[0] % 32)` -> `return false;`
- L1950: `case GGML_TYPE_F16: if (!opt_experimental)` -> `return false;`
- L1989: `case GGML_TYPE_Q4_0: case GGML_TYPE_Q8_0: case GGML_TYPE_MXFP4: if ((src0->ne[0] % 32))` -> `return false;`
- L2001: `case GGML_TYPE_F16: if (!opt_experimental)` -> `return false;`

## src\ggml-opencl\ggml-opencl.cpp
- L9034: `case GGML_OP_GET_ROWS: if (!any_on_device)` -> `return false;`
- L9040: `case GGML_OP_SET_ROWS: if (!any_on_device)` -> `return false;`
- L9046: `case GGML_OP_CPY: if (!any_on_device)` -> `return false;`
- L9052: `case GGML_OP_DUP: case GGML_OP_CONT: if (!any_on_device)` -> `return false;`
- L9059: `case GGML_OP_ADD: if (!any_on_device)` -> `return false;`
- L9065: `case GGML_OP_ADD_ID: if (!any_on_device)` -> `return false;`
- L9071: `case GGML_OP_MUL: if (!any_on_device)` -> `return false;`
- L9077: `case GGML_OP_DIV: if (!any_on_device)` -> `return false;`
- L9083: `case GGML_OP_SUB: if (!any_on_device)` -> `return false;`
- L9091: `case GGML_UNARY_OP_GELU: if (!any_on_device)` -> `return false;`
- L9097: `case GGML_UNARY_OP_GELU_ERF: if (!any_on_device)` -> `return false;`
- L9103: `case GGML_UNARY_OP_GELU_QUICK: if (!any_on_device)` -> `return false;`
- L9109: `case GGML_UNARY_OP_SILU: if (!any_on_device)` -> `return false;`
- L9115: `case GGML_UNARY_OP_RELU: if (!any_on_device)` -> `return false;`
- L9121: `case GGML_UNARY_OP_SIGMOID: if (!any_on_device)` -> `return false;`
- L9127: `case GGML_UNARY_OP_TANH: if (!any_on_device)` -> `return false;`
- L9136: `case GGML_OP_GLU: if (!any_on_device)` -> `return false;`
- L9142: `case GGML_OP_CLAMP: if (!any_on_device)` -> `return false;`
- L9148: `case GGML_OP_NORM: if (!any_on_device)` -> `return false;`
- L9154: `case GGML_OP_RMS_NORM: if (!any_on_device)` -> `return false;`
- L9160: `case GGML_OP_GROUP_NORM: if (!any_on_device)` -> `return false;`
- L9166: `case GGML_OP_REPEAT: if (!any_on_device)` -> `return false;`
- L9172: `case GGML_OP_PAD: if (!any_on_device)` -> `return false;`
- L9178: `case GGML_OP_UPSCALE: if (!any_on_device)` -> `return false;`
- L9184: `case GGML_OP_CONV_2D: if (!any_on_device)` -> `return false;`
- L9190: `case GGML_OP_CONCAT: if (!any_on_device)` -> `return false;`
- L9196: `case GGML_OP_TIMESTEP_EMBEDDING: if (!any_on_device)` -> `return false;`
- L9202: `case GGML_OP_MUL_MAT: if (!any_on_device && !ggml_cl_can_mul_mat(tensor->src[0], tensor->src[1], tensor))` -> `return false;`
- L9208: `case GGML_OP_MUL_MAT_ID: if (!any_on_device)` -> `return false;`
- L9214: `case GGML_OP_SCALE: if (!any_on_device)` -> `return false;`
- L9220: `case GGML_OP_RESHAPE: case GGML_OP_VIEW: case GGML_OP_PERMUTE: case GGML_OP_TRANSPOSE: if (!any_on_device)` -> `return false;`
- L9229: `case GGML_OP_DIAG_MASK_INF: if (!any_on_device)` -> `return false;`
- L9235: `case GGML_OP_SOFT_MAX: if (!any_on_device)` -> `return false;`
- L9241: `case GGML_OP_ROPE: if (!any_on_device)` -> `return false;`
- L9247: `case GGML_OP_IM2COL: if (!any_on_device)` -> `return false;`
- L9253: `case GGML_OP_ARGSORT: if (!any_on_device)` -> `return false;`
- L9259: `case GGML_OP_SUM_ROWS: if (!any_on_device)` -> `return false;`
- L9265: `case GGML_OP_FLASH_ATTN_EXT: if (!any_on_device)` -> `return false;`

## src\ggml-sycl\dpct\helper.hpp
- L662: `int is_native_atomic_supported()` -> `return 0;`

## src\ggml-sycl\ggml-sycl.cpp
- L3845: `case GGML_OP_MUL_MAT: if (dst->src[0]->ne[3] != dst->src[1]->ne[3])` -> `return false;`
- L3852: `case GGML_OP_MUL_MAT_ID: if (dst->src[0]->ne[3] != dst->src[1]->ne[3])` -> `return false;`

## src\ggml-vulkan\ggml-vulkan.cpp
- L8030: `case GGML_OP_ADD: case GGML_OP_SUB: case GGML_OP_MUL: case GGML_OP_DIV: if ((src0->type != GGML_TYPE_F32 && src0->type != GGML_TYPE_F16) || (src1->type != GGML_TYPE_F32 && src1->type != GGML_TYPE_F16) || (dst->type != GGML_TYPE_F32 && dst->type != GGML_TYPE_F16))` -> `return nullptr;`
- L8207: `case GGML_OP_UNARY: if ((src0->type != GGML_TYPE_F32 && src0->type != GGML_TYPE_F16) || (dst->type != GGML_TYPE_F32 && dst->type != GGML_TYPE_F16) || (src0->type != dst->type))` -> `return nullptr;`
- L8243: `case GGML_OP_GLU: if ((src0->type != GGML_TYPE_F32 && src0->type != GGML_TYPE_F16) || (dst->type != GGML_TYPE_F32 && dst->type != GGML_TYPE_F16) || (src0->type != dst->type))` -> `return nullptr;`

## src\hybrid_cloud_manager.cpp
- L1162: `void* HybridCloudManager::createRequestPayload(const ExecutionRequest& request, const std::string& providerId)` -> `return nullptr;`

## src\intelligent_codebase_engine.cpp
- L364: `std::vector<std::string> IntelligentCodebaseEngine::getCircularDependencies()` -> `return {};`

## src\masm\MASMCompilerWidget.cpp
- L91: `int MASMCodeEditor::lineNumberAreaWidth()` -> `return 0;`
- L307: `stringList MASMCompilerWidget::getCompilerArguments(const std::string& sourceFile, const std::string& outputFile) const` -> `return {};`

## src\model_interface.cpp
- L290: `void* ModelInterface::getUsageStatistics() const` -> `return nullptr;`
- L292: `void* ModelInterface::getModelStats(const std::string& model_name) const` -> `return nullptr;`
- L294: `int ModelInterface::getSuccessRate(const std::string& model_name) const` -> `return 0;`
- L297: `void* ModelInterface::getCostBreakdown() const` -> `return nullptr;`
- L323: `void* ModelInterface::getModelListAsJson() const` -> `return nullptr;`

## src\modules\quickjs_extension_host.cpp
- L110: `size_t QuickJSExtensionHost::getLoadedExtensionCount() const` -> `return 0;`
- L111: `size_t QuickJSExtensionHost::getActiveExtensionCount() const` -> `return 0;`

## src\modules\unreal_engine_integration.cpp
- L1011: `bool UnrealEngineIntegration::isHotReloadInProgress() const` -> `return false;`
- L1241: `std::vector<UnrealDebugStackFrame> UnrealEngineIntegration::getCallStack() const` -> `return {};`
- L1243: `std::vector<UnrealDebugVariable> UnrealEngineIntegration::getLocals(int frameId) const` -> `return {};`
- L1244: `std::string UnrealEngineIntegration::evaluateExpression(const std::string& expr, int frameId) const` -> `return "";`
- L1365: `bool UnrealEngineIntegration::isSourceControlEnabled() const` -> `return false;`

## src\modules\vscode_extension_api.cpp
- L1170: `bool isTelemetryEnabled()` -> `return false;`

## src\monaco_integration.h
- L36: `std::string getCurrentFile() const` -> `return "";`

## src\native_agent.hpp
- L218: `std::string PerformResearch(const std::string& query)` -> `return "";`

## src\net\net_impl_win32.cpp
- L1256: `size_t ConnectionPool::getActiveConnections() const` -> `return 0;`
- L1258: `size_t ConnectionPool::getIdleConnections() const` -> `return 0;`

## src\production_agentic_ide.cpp
- L620: `LRESULT ProductionAgenticIDE::OnCreate()` -> `return 0;`

## src\QtGUIStubs.hpp
- L50: `bool hasFocus() const` -> `return false;`
- L54: `void* parentWidget() const` -> `return nullptr;`
- L57: `int width() const` -> `return 0;`
- L59: `int height() const` -> `return 0;`
- L80: `void* centralWidget() const` -> `return nullptr;`
- L91: `class void* statusBar()` -> `return nullptr;`
- L114: `int count() const` -> `return 0;`
- L116: `QLayoutItem* itemAt(int index)` -> `return nullptr;`
- L183: `bool isReadOnly() const` -> `return false;`
- L198: `bool isReadOnly() const` -> `return false;`
- L212: `bool isChecked() const` -> `return false;`
- L220: `bool isChecked() const` -> `return false;`
- L230: `bool isChecked() const` -> `return false;`
- L243: `int count() const` -> `return 0;`
- L245: `int currentIndex() const` -> `return 0;`
- L255: `int value() const` -> `return 0;`
- L278: `int value() const` -> `return 0;`
- L289: `int value() const` -> `return 0;`
- L305: `int addTab(void* w, const std::string& label)` -> `return 0;`
- L307: `int insertTab(int index, void* w, const std::string& label)` -> `return 0;`
- L309: `int count() const` -> `return 0;`
- L311: `int currentIndex() const` -> `return 0;`
- L313: `void* widget(int index)` -> `return nullptr;`
- L315: `void* currentWidget()` -> `return nullptr;`
- L324: `bool isValid() const` -> `return false;`
- L340: `int childCount() const` -> `return 0;`
- L342: `QTreeWidgetItem* child(int index)` -> `return nullptr;`
- L343: `QTreeWidgetItem* parent()` -> `return nullptr;`
- L352: `int topLevelItemCount() const` -> `return 0;`
- L354: `QTreeWidgetItem* topLevelItem(int index)` -> `return nullptr;`
- L358: `QTreeWidgetItem* currentItem()` -> `return nullptr;`
- L384: `int count() const` -> `return 0;`
- L386: `QListWidgetItem* item(int row)` -> `return nullptr;`
- L387: `QListWidgetItem* currentItem()` -> `return nullptr;`
- L398: `class QTableWidgetItem* item(int row, int col)` -> `return nullptr;`
- L411: `int count() const` -> `return 0;`
- L413: `void* widget(int index)` -> `return nullptr;`
- L417: `std::vector<int> sizes() const` -> `return {};`
- L425: `void* widget() const` -> `return nullptr;`
- L459: `bool isCheckable() const` -> `return false;`
- L485: `bool isChecked() const` -> `return false;`
- L501: `void* addAction(const std::string& text)` -> `return nullptr;`
- L503: `void* addAction(const void& icon, const std::string& text)` -> `return nullptr;`
- L519: `void* addMenu(const std::string& title)` -> `return nullptr;`
- L521: `void* addMenu(const void& icon, const std::string& title)` -> `return nullptr;`
- L523: `void* addAction(const std::string& text)` -> `return nullptr;`
- L531: `void* addAction(const std::string& text)` -> `return nullptr;`
- L533: `void* addAction(const void& icon, const std::string& text)` -> `return nullptr;`
- L564: `int result() const` -> `return 0;`
- L567: `int exec()` -> `return 0;`
- L631: `static void* instance()` -> `return nullptr;`
- L633: `int exec()` -> `return 0;`
- L639: `static QStyle* style()` -> `return nullptr;`
- L666: `int button() const` -> `return 0;`
- L670: `public: int key() const` -> `return 0;`
- L673: `bool isAutoRepeat() const` -> `return false;`
- L677: `public: int delta() const` -> `return 0;`

## src\quick_stubs.cpp
- L7: `bool LSP_Initialize()` -> `return false;`
- L11: `std::string LSP_GetDiagnostics()` -> `return "";`

## src\rawrxd_transformer.hpp
- L373: `uint16_t* GetKCache(uint32_t layer, uint32_t head, size_t pos)` -> `return nullptr;`

## src\RawrXD_UndoStack.h
- L17: `virtual bool mergeWith(const UndoCommand* other)` -> `return false;`

## src\stubs\production_link_stubs.cpp
- L31: `std::vector<ModelVersion> getAllModels() const` -> `return {};`
- L108: `std::string CheckpointManager::saveCheckpoint(const CheckpointMetadata&, const CheckpointState&, CompressionLevel)` -> `return {};`
- L109: `std::string CheckpointManager::quickSaveCheckpoint(const CheckpointMetadata&, const CheckpointState&)` -> `return {};`
- L110: `std::string CheckpointManager::saveModelWeights(const CheckpointMetadata&, const std::vector<uint8_t>&, CompressionLevel)` -> `return {};`
- L111: `bool CheckpointManager::loadCheckpoint(const std::string&, CheckpointState&)` -> `return false;`
- L112: `std::string CheckpointManager::loadLatestCheckpoint(CheckpointState&)` -> `return {};`
- L113: `std::string CheckpointManager::loadBestCheckpoint(CheckpointState&)` -> `return {};`
- L114: `std::string CheckpointManager::loadCheckpointFromEpoch(int, CheckpointState&)` -> `return {};`
- L115: `CheckpointManager::CheckpointMetadata CheckpointManager::getCheckpointMetadata(const std::string&) const` -> `return {};`
- L116: `std::vector<CheckpointManager::CheckpointIndex> CheckpointManager::listCheckpoints() const` -> `return {};`
- L117: `std::vector<CheckpointManager::CheckpointIndex> CheckpointManager::getCheckpointHistory(int) const` -> `return {};`
- L118: `bool CheckpointManager::deleteCheckpoint(const std::string&)` -> `return false;`
- L119: `int CheckpointManager::pruneOldCheckpoints(int)` -> `return 0;`
- L120: `CheckpointManager::CheckpointMetadata CheckpointManager::getBestCheckpointInfo() const` -> `return {};`
- L121: `bool CheckpointManager::updateCheckpointMetadata(const std::string&, const CheckpointMetadata&)` -> `return false;`
- L122: `bool CheckpointManager::setCheckpointNote(const std::string&, const std::string&)` -> `return false;`
- L123: `bool CheckpointManager::enableAutoCheckpointing(int, int)` -> `return false;`
- L125: `bool CheckpointManager::shouldCheckpoint(int, int) const` -> `return false;`
- L126: `bool CheckpointManager::validateCheckpoint(const std::string&) const` -> `return false;`
- L128: `bool CheckpointManager::repairCheckpoint(const std::string&)` -> `return false;`
- L129: `uint64_t CheckpointManager::getTotalCheckpointSize() const` -> `return 0;`
- L130: `uint64_t CheckpointManager::getCheckpointSize(const std::string&) const` -> `return 0;`
- L131: `std::string CheckpointManager::generateCheckpointReport() const` -> `return {};`
- L132: `std::string CheckpointManager::compareCheckpoints(const std::string&, const std::string&) const` -> `return {};`
- L134: `bool CheckpointManager::synchronizeDistributedCheckpoints()` -> `return false;`
- L135: `std::string CheckpointManager::exportConfiguration() const` -> `return {};`
- L136: `bool CheckpointManager::importConfiguration(const std::string&)` -> `return false;`
- L137: `bool CheckpointManager::saveConfigurationToFile(const std::string&) const` -> `return false;`
- L138: `bool CheckpointManager::loadConfigurationFromFile(const std::string&)` -> `return false;`
- L139: `std::string CheckpointManager::generateCheckpointId()` -> `return {};`
- L140: `std::vector<uint8_t> CheckpointManager::compressState(const std::vector<uint8_t>&, CompressionLevel)` -> `return {};`
- L141: `std::vector<uint8_t> CheckpointManager::decompressState(const std::vector<uint8_t>&)` -> `return {};`
- L142: `bool CheckpointManager::writeCheckpointToDisk(const std::string&, const CheckpointState&, CompressionLevel)` -> `return false;`
- L143: `bool CheckpointManager::readCheckpointFromDisk(const std::string&, CheckpointState&)` -> `return false;`
- L232: `int verifyCursorParityWiring(void*)` -> `return 0;`

## src\token_generator.cpp
- L405: `json TokenGenerator::getStatus() const` -> `return {};`

## src\tools\license_validator_telemetry_stub.cpp
- L19: `uint64_t UTC_LogEvent(const char* /*message*/)` -> `return 0;`

## src\win32app\agentic_bridge_headless.cpp
- L29: `bool AgenticBridge::Initialize(const std::string&, const std::string&)` -> `return false;`
- L36: `bool AgenticBridge::StartAgentLoop(const std::string&, int)` -> `return false;`
- L39: `std::vector<std::string> AgenticBridge::GetAvailableTools()` -> `return {};`
- L49: `bool AgenticBridge::LoadModel(const std::string&)` -> `return false;`
- L62: `SubAgentManager* AgenticBridge::GetSubAgentManager()` -> `return nullptr;`
- L76: `bool AgenticBridge::DispatchModelToolCalls(const std::string&, std::string&)` -> `return false;`

## src\win32app\ask_mode_handler.hpp
- L16: `inline std::string AskModeUserPrefix()` -> `return "";`


