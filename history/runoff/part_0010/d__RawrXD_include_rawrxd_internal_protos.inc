; ═══════════════════════════════════════════════════════════════════════════════
; rawrxd_internal_protos.inc - Cross-Module EXTERNDEF Declarations (x64)
; ═══════════════════════════════════════════════════════════════════════════════
; Resolves ALL undefined internal CALL targets across the RawrXD codebase.
; Each EXTERNDEF declares a symbol that exists in another .asm/.obj file.
; At link time, the linker resolves these cross-references.
;
; This also serves as the PUBLIC contract: every PROC listed here MUST have a
; matching PUBLIC declaration in its source file (see rawrxd_public_exports.inc).
;
; Usage:  INCLUDE rawrxd_internal_protos.inc
; ═══════════════════════════════════════════════════════════════════════════════

IFNDEF RAWRXD_INTERNAL_PROTOS_INC
RAWRXD_INTERNAL_PROTOS_INC EQU 1

; ═══════════════════════════════════════════════════════════════════════════════
; AI / Inference Engine
; Source: RawrXD_Standalone_Inference.asm, RawrXD_InferenceCore.asm,
;         rawrxd_complete_master_implementation.asm, Titan_InferenceCore.asm
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF AI_RunAttentionLayer:PROC
EXTERNDEF AI_RunFFNLayer:PROC
EXTERNDEF AI_RunLayerNorm:PROC
EXTERNDEF AI_Inference_Execute:PROC
EXTERNDEF AI_MatMul_QKV:PROC
EXTERNDEF AI_MultiHead_Attention:PROC
EXTERNDEF AI_RunInference:PROC
EXTERNDEF AI_GetError:PROC
EXTERNDEF AI_CHECK_HRESULT:PROC
EXTERNDEF AI_CHECK_VULKAN:PROC
EXTERNDEF ApplyTemperatureReal:PROC
EXTERNDEF ApplyRoPE:PROC
EXTERNDEF Attention:PROC
EXTERNDEF Attention_Compute:PROC
EXTERNDEF Attention_Softmax:PROC
EXTERNDEF CacheTokensWithConfidence:PROC
EXTERNDEF DraftModel_Forward:PROC
EXTERNDEF Embedding_LookupReal:PROC
EXTERNDEF FeedForward:PROC
EXTERNDEF FFN:PROC
EXTERNDEF Float32ToFloat16:PROC
EXTERNDEF ForwardPass:PROC
EXTERNDEF GenerateTokens:PROC
EXTERNDEF InferenceCore_GenerateToken:PROC
EXTERNDEF InferenceCore_Initialize:PROC
EXTERNDEF InferenceCore_LoadModel:PROC
EXTERNDEF InferenceCore_RunInference:PROC
EXTERNDEF InferenceCore_Tokenize_AVX2:PROC
EXTERNDEF InferenceEngine_LoadModel:PROC
EXTERNDEF InferenceEngine_ProcessChat:PROC
EXTERNDEF Inference_FreeSequence:PROC
EXTERNDEF Inference_GetKVContextSize:PROC
EXTERNDEF Inference_MatMul_AVX512:PROC
EXTERNDEF Inference_SubmitBatch:PROC
EXTERNDEF InitInferenceEngine:PROC
EXTERNDEF Linear_ProjectionReal:PROC
EXTERNDEF NeuralPredictTokens:PROC
EXTERNDEF NeuralTokenize:PROC
EXTERNDEF OptimizedQuantizeTensor:PROC
EXTERNDEF QuantizeAuto:PROC
EXTERNDEF RMSNorm_ForwardReal:PROC
EXTERNDEF RMSNorm_AVX512:PROC
EXTERNDEF RoPE:PROC
EXTERNDEF RoPE_Rotate_AVX512:PROC
EXTERNDEF RunInferenceStep:PROC
EXTERNDEF Softmax_InPlace:PROC
EXTERNDEF Token_Generate:PROC
EXTERNDEF InferStream:PROC
EXTERNDEF RunLocalModel:PROC

; ─── Speculative Decoding ─────────────────────────────────────────────────────
EXTERNDEF SpeculativeDecoder_GenerateDrafts:PROC
EXTERNDEF SpeculativeDecoder_Init:PROC
EXTERNDEF SpeculativeDecoder_Verify:PROC

; ─── KV Cache ─────────────────────────────────────────────────────────────────
EXTERNDEF KVCache_Append:PROC
EXTERNDEF KVCache_Create:PROC
EXTERNDEF KVCache_Get:PROC
EXTERNDEF KVCache_GetStats:PROC
EXTERNDEF KVCache_Init:PROC
EXTERNDEF KVCache_Insert:PROC
EXTERNDEF KVCache_Lookup:PROC
EXTERNDEF AllocateKVSlot:PROC
EXTERNDEF ComputeQKV:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; GGUF / Safetensors / Model Loading
; Source: rawrxd_complete_master_implementation.asm, Phase2_Master.asm
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF GGUF_FindTensorInfo:PROC
EXTERNDEF GGUF_ParseHeader:PROC
EXTERNDEF GGML_ContextCleanup:PROC
EXTERNDEF Safetensors_Init:PROC
EXTERNDEF Safetensors_Load:PROC
EXTERNDEF Safetensors_ParseHeader:PROC
EXTERNDEF PyTorch_Load:PROC
EXTERNDEF ONNX_Load:PROC
EXTERNDEF DetectModelFormat:PROC
EXTERNDEF LoadModelNative:PROC
EXTERNDEF UnloadModelNative:PROC
EXTERNDEF StreamingGGUF_Init:PROC
EXTERNDEF StreamingGGUF_LoadTensor:PROC
EXTERNDEF ModelRouter_Init:PROC
EXTERNDEF ModelRouter_SelectModel:PROC
EXTERNDEF ModelRouter_UpdateScore:PROC
EXTERNDEF ModelState_Transition:PROC
EXTERNDEF RouteModelLoad:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; Titan Model Loader / Orchestrator
; Source: RawrXD_Titan.asm, RawrXD_Titan_CORE.asm, Titan_InferenceCore.asm
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF Titan_AlignedAlloc:PROC
EXTERNDEF Titan_AllocateKVCacheDeferred:PROC
EXTERNDEF Titan_AllocateRingBufferSpace:PROC
EXTERNDEF Titan_AllocateSlot_Production:PROC
EXTERNDEF Titan_BeginStreamingInference:PROC
EXTERNDEF Titan_BeginStreamingInference_Native:PROC
EXTERNDEF Titan_Bootstrap_Sieve_Real:PROC
EXTERNDEF Titan_BuildTensorTable:PROC
EXTERNDEF Titan_CheckHeartbeats:PROC
EXTERNDEF Titan_CleanupAllModels:PROC
EXTERNDEF Titan_CleanupRingBuffer:PROC
EXTERNDEF Titan_ConsumeToken:PROC
EXTERNDEF Titan_CreateContext:PROC
EXTERNDEF Titan_DecompressNF4:PROC
EXTERNDEF Titan_DequeueTask:PROC
EXTERNDEF Titan_DetectConflict_Production:PROC
EXTERNDEF Titan_DetectConflict_Simplified:PROC
EXTERNDEF Titan_DirectStorage_Init_Real:PROC
EXTERNDEF Titan_DirectStorage_Shutdown:PROC
EXTERNDEF Titan_DirectStorage_Submit:PROC
EXTERNDEF Titan_Dispatch_Nitro_Shader_Real:PROC
EXTERNDEF Titan_Emergency_Cleanup:PROC
EXTERNDEF Titan_Evict_LRU_Slot_Real:PROC
EXTERNDEF Titan_ExecuteAttentionTask:PROC
EXTERNDEF Titan_ExecuteMatMulTask:PROC
EXTERNDEF Titan_ExecuteNormTask:PROC
EXTERNDEF Titan_ExtractArchitecture:PROC
EXTERNDEF Titan_FindFreeModelSlot:PROC
EXTERNDEF Titan_GetDMAStats:PROC
EXTERNDEF Titan_GetGPUStats:PROC
EXTERNDEF Titan_GetHeartbeatStats:PROC
EXTERNDEF Titan_GhostCacheLookup:PROC
EXTERNDEF Titan_HeartbeatCallback:PROC
EXTERNDEF Titan_InferenceThread:PROC
EXTERNDEF Titan_InitializeDMA:PROC
EXTERNDEF Titan_InitializeGPU:PROC
EXTERNDEF Titan_InitOrchestrator_Production:PROC
EXTERNDEF Titan_InitOrchestrator_Simplified:PROC
EXTERNDEF Titan_IsDeviceAddress:PROC
EXTERNDEF Titan_IsHeartbeatAlive:PROC
EXTERNDEF Titan_LoadModelPersistent:PROC
EXTERNDEF Titan_LoadModel_GGUF:PROC
EXTERNDEF Titan_LogError:PROC
EXTERNDEF Titan_LRU_Register_Slot:PROC
EXTERNDEF Titan_LRU_Touch_Slot:PROC
EXTERNDEF Titan_Master_Shutdown:PROC
EXTERNDEF Titan_ParseTensorInfos:PROC
EXTERNDEF Titan_PerformDMA:PROC
EXTERNDEF Titan_PredictNextLayer:PROC
EXTERNDEF Titan_Queue_Submit:PROC
EXTERNDEF Titan_ReleaseRingBufferSpace:PROC
EXTERNDEF Titan_ReleaseSlot_Production:PROC
EXTERNDEF Titan_ResetGPUStats:PROC
EXTERNDEF Titan_RMSNorm_AVX512:PROC
EXTERNDEF Titan_ShutdownDMA:PROC
EXTERNDEF Titan_Shutdown_Real:PROC
EXTERNDEF Titan_Softmax_AVX512:PROC
EXTERNDEF Titan_StopInference:PROC
EXTERNDEF Titan_SubmitJob:PROC
EXTERNDEF Titan_TestMain:PROC
EXTERNDEF Titan_UpdateHeartbeat:PROC
EXTERNDEF Titan_UpdateHeartbeat_Production:PROC
EXTERNDEF Titan_UpdateHeartbeat_Simplified:PROC
EXTERNDEF Titan_Validate_Memory_State:PROC
EXTERNDEF Titan_Vulkan_BindSparseMemory:PROC
EXTERNDEF Titan_Vulkan_EmergencyReset:PROC
EXTERNDEF Titan_Vulkan_Init_Real:PROC
EXTERNDEF Titan_Vulkan_Shutdown:PROC
EXTERNDEF Titan_WaitInferenceComplete:PROC
EXTERNDEF Titan_WorkerThread:PROC
EXTERNDEF Titan_WorkerThreadProc:PROC
EXTERNDEF TITAN_CheckGhostCache:PROC
EXTERNDEF TITAN_Initialize:PROC
EXTERNDEF TITAN_SyncLiveTheta:PROC
EXTERNDEF TITAN_UpdatePredictor:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; GPU / Compute Backend
; Source: gpu_backend.asm, cuda_api.asm, gpu_memory.asm, gpu_detection.asm
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF AllocateCPUMemory:PROC
EXTERNDEF AllocateCUDAMemory:PROC
EXTERNDEF AllocateROCmMemory:PROC
EXTERNDEF AllocateTensorMemory:PROC
EXTERNDEF AllocateVulkanMemory:PROC
EXTERNDEF CPU_CheckAVX512:PROC
EXTERNDEF DetectAMDGPU:PROC
EXTERNDEF DetectGPUs:PROC
EXTERNDEF DetectIntelGPU:PROC
EXTERNDEF DetectNVIDIAGPU:PROC
EXTERNDEF ExecuteCPUKernel:PROC
EXTERNDEF ExecuteCUDAKernel:PROC
EXTERNDEF ExecuteROCmKernel:PROC
EXTERNDEF ExecuteVulkanKernel:PROC
EXTERNDEF FreeCPUMemory:PROC
EXTERNDEF FreeCUDAMemory:PROC
EXTERNDEF FreeROCmMemory:PROC
EXTERNDEF FreeVulkanMemory:PROC
EXTERNDEF InitializeCPUBackend:PROC
EXTERNDEF InitializeCUDABackend:PROC
EXTERNDEF InitializeROCmBackend:PROC
EXTERNDEF InitializeVulkanBackend:PROC
EXTERNDEF GPU_Detect:PROC
EXTERNDEF GPU_GetAMDProperties:PROC
EXTERNDEF GPU_GetDevice:PROC
EXTERNDEF GPU_GetDeviceCount:PROC
EXTERNDEF GPU_GetDeviceProperties:PROC
EXTERNDEF GPU_GetGenericProperties:PROC
EXTERNDEF GPU_GetIntelProperties:PROC
EXTERNDEF GPU_GetNVIDIAProperties:PROC
EXTERNDEF GPU_QuantizedMatMul:PROC
EXTERNDEF GPU_ReadMemorySize:PROC
EXTERNDEF GPU_ReadPCIConfig:PROC
EXTERNDEF GPU_ReadPCIConfigDword:PROC
EXTERNDEF GPU_ScanPCIBus:PROC
EXTERNDEF GPU_SubmitDMATransfer:PROC
EXTERNDEF GPU_WaitForDMA:PROC
EXTERNDEF VulkanCompute_ExecuteMatMul:PROC
EXTERNDEF VulkanCompute_Shutdown:PROC
EXTERNDEF Vulkan_CreateComputePipeline:PROC
EXTERNDEF Vulkan_DestroyBuffers:PROC
EXTERNDEF Vulkan_DispatchCompute:PROC
EXTERNDEF Vulkan_InitializeReal:PROC
EXTERNDEF Vulkan_ResetDescriptorPool:PROC
EXTERNDEF VulkanInitialize:PROC
EXTERNDEF VulkanFindMemoryType:PROC
EXTERNDEF VulkanMapBuffer:PROC
EXTERNDEF VulkanUnmapBuffer:PROC
EXTERNDEF VulkanCreatePoolsAndSyncs:PROC
EXTERNDEF VulkanBindFSMDescriptors:PROC
EXTERNDEF VulkanCreateFSMBuffer_ASM:PROC
EXTERNDEF VulkanFSMCreatePipeline:PROC
EXTERNDEF VulkanFSMUpdate_ASM:PROC
EXTERNDEF VulkanRegisterFabricCoordination:PROC
EXTERNDEF BitmaskBroadcastVulkan:PROC
EXTERNDEF BitmaskBroadcast_ASM:PROC

; ─── MatMul Kernels ───────────────────────────────────────────────────────────
EXTERNDEF MatMul_AVX:PROC
EXTERNDEF MatMul_F16_AVX512:PROC
EXTERNDEF MatMul_Q2_K_F32:PROC
EXTERNDEF MatMul_Q3_K_F32:PROC
EXTERNDEF MatMul_Q4_0_F32:PROC
EXTERNDEF MatMul_Q4_1_F32:PROC
EXTERNDEF MatMul_Q4_K_F32:PROC
EXTERNDEF MatMul_Q5_0_F32:PROC
EXTERNDEF MatMul_Q5_1_F32:PROC
EXTERNDEF MatMul_Q5_K_F32:PROC
EXTERNDEF MatMul_Q6_K_F32:PROC
EXTERNDEF MatMul_Q8_0_F32:PROC

; ─── Quantization ─────────────────────────────────────────────────────────────
EXTERNDEF AllocateTensor:PROC
EXTERNDEF DequantizeTensor:PROC
EXTERNDEF DequantizeTensor_Q4_K:PROC
EXTERNDEF Dequantize_Q4_0_Block:PROC
EXTERNDEF QuantizeTensor_Q4_K:PROC
EXTERNDEF Quant_Q2K_Deblock:PROC
EXTERNDEF NF4_Decompress_Full:PROC
EXTERNDEF NF4_Decompress_Grouped:PROC
EXTERNDEF NF4_Decompress_Sparse:PROC
EXTERNDEF DecompressNF4_AVX512Hidden:PROC
EXTERNDEF Tensor_QuantizedMatMul:PROC
EXTERNDEF ProcessTensorBatch:PROC
EXTERNDEF TensorData_ReleaseRef:PROC
EXTERNDEF RawrXD_HierarchicalQuant:PROC
EXTERNDEF RawrXD_GetQuantStats:PROC
EXTERNDEF SetTensorCount:PROC
EXTERNDEF GetTensorByName:PROC
EXTERNDEF HotpatchTensorOperation:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; DAP Debugger Protocol
; Source: RawrXD_DAP.asm
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF DAPInit:PROC
EXTERNDEF DAPContinue:PROC
EXTERNDEF DAPLaunch:PROC
EXTERNDEF DAPSetBreakpoint:PROC
EXTERNDEF DAPStackTrace:PROC
EXTERNDEF DAPStepOver:PROC
EXTERNDEF DAP_ConvertStackFrames:PROC
EXTERNDEF DAP_CreateBreakpoint:PROC
EXTERNDEF DAP_CreateDebuggeeProcess:PROC
EXTERNDEF DAP_CreateLocalsScope:PROC
EXTERNDEF DAP_CreateRegistersScope:PROC
EXTERNDEF DAP_GetLocalVariables:PROC
EXTERNDEF DAP_GetRegisterVariables:PROC
EXTERNDEF DAP_GetThreadContext:PROC
EXTERNDEF DAP_GetThreadHandle:PROC
EXTERNDEF DAP_HandleCreateProcess:PROC
EXTERNDEF DAP_HandleCreateThread:PROC
EXTERNDEF DAP_HandleException:PROC
EXTERNDEF DAP_HandleExitProcess:PROC
EXTERNDEF DAP_HandleExitThread:PROC
EXTERNDEF DAP_HandleLoadDll:PROC
EXTERNDEF DAP_HandleOutputString:PROC
EXTERNDEF DAP_HandleRip:PROC
EXTERNDEF DAP_HandleUnloadDll:PROC
EXTERNDEF DAP_InitCapabilities:PROC
EXTERNDEF DAP_ProcessDebugEvent:PROC
EXTERNDEF DAP_ResolveSourceLocation:PROC
EXTERNDEF DAP_SendBreakpointEvent:PROC
EXTERNDEF DAP_SendEvent:PROC
EXTERNDEF DAP_SendInitializedEvent:PROC
EXTERNDEF DAP_SendThreadEvent:PROC
EXTERNDEF DAP_SerializeEvent:PROC
EXTERNDEF DAP_SetupDebugEventLoop:PROC
EXTERNDEF DAP_VerifyBreakpoint:PROC
EXTERNDEF DAP_WalkStackFrames:PROC

; ─── Debugger Core ────────────────────────────────────────────────────────────
EXTERNDEF Debugger_Attach:PROC
EXTERNDEF Debugger_GetRegisters:PROC
EXTERNDEF Debugger_Init:PROC
EXTERNDEF Debugger_RemoveBreakpoint:PROC
EXTERNDEF Debugger_SetBreakpoint:PROC
EXTERNDEF Debugger_StepOver:PROC
EXTERNDEF Debugger_WaitForEvent:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; Extension Host / Marketplace
; Source: RawrXD_ExtensionHost.asm, RawrXD_MarketplaceInstaller.asm
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF CompletionProvider_Adapter_Create:PROC
EXTERNDEF DefinitionProvider_Adapter_Create:PROC
EXTERNDEF DisposableCollection_Create:PROC
EXTERNDEF DisposableCollection_Dispose:PROC
EXTERNDEF Disposable_Create:PROC
EXTERNDEF EventEmitter_Create:PROC
EXTERNDEF EventFire_ExtensionActivated:PROC
EXTERNDEF EventFire_ExtensionDeactivated:PROC
EXTERNDEF EventListener_DisposeInternal:PROC
EXTERNDEF ExtensionContext_Create:PROC
EXTERNDEF ExtensionHostBridge_Create:PROC
EXTERNDEF ExtensionHostBridge_ProcessMessages:PROC
EXTERNDEF ExtensionHostBridge_RegisterWebview:PROC
EXTERNDEF ExtensionHostBridge_SendMessage:PROC
EXTERNDEF ExtensionHostBridge_SendNotification:PROC
EXTERNDEF ExtensionHostBridge_SendRequest:PROC
EXTERNDEF ExtensionHostBridge_SignalReady:PROC
EXTERNDEF ExtensionHost_BuildAPI:PROC
EXTERNDEF ExtensionHost_HandleActivateRequest:PROC
EXTERNDEF ExtensionHost_HandleDeactivateRequest:PROC
EXTERNDEF ExtensionHost_LoadExtensions:PROC
EXTERNDEF ExtensionHost_MessageLoop:PROC
EXTERNDEF ExtensionHost_Shutdown:PROC
EXTERNDEF ExtensionManifest_FromJson:PROC
EXTERNDEF ExtensionModule_Load:PROC
EXTERNDEF ExtensionStorage_GetPath:PROC
EXTERNDEF Extension_CheckAutoActivate:PROC
EXTERNDEF Extension_CleanupLanguageClients:PROC
EXTERNDEF Extension_CleanupWebviews:PROC
EXTERNDEF Extension_Create:PROC
EXTERNDEF Extension_GetCurrent:PROC
EXTERNDEF Extension_Register:PROC
EXTERNDEF Extension_ValidateCapabilities:PROC
EXTERNDEF HoverProvider_Adapter_Create:PROC
EXTERNDEF Marketplace_CheckVersionCompatibility:PROC
EXTERNDEF Marketplace_DetermineExtensionType:PROC
EXTERNDEF Marketplace_DownloadExtension:PROC
EXTERNDEF Marketplace_ExtractFiles:PROC
EXTERNDEF Marketplace_ExtractManifestFields:PROC
EXTERNDEF Marketplace_ExtractSingleFile:PROC
EXTERNDEF Marketplace_GenerateActivateFunction:PROC
EXTERNDEF Marketplace_GenerateApiExports:PROC
EXTERNDEF Marketplace_GenerateAsmHeader:PROC
EXTERNDEF Marketplace_GenerateAsmIncludes:PROC
EXTERNDEF Marketplace_GenerateDeactivateFunction:PROC
EXTERNDEF Marketplace_GenerateExtensionInfo:PROC
EXTERNDEF Marketplace_GenerateInstallDir:PROC
EXTERNDEF Marketplace_GenerateNativeEntryPoint:PROC
EXTERNDEF Marketplace_InitializeContext:PROC
EXTERNDEF Marketplace_InstallDependency:PROC
EXTERNDEF Marketplace_InstallExtension:PROC
EXTERNDEF Marketplace_IsExtensionInstalled:PROC
EXTERNDEF Marketplace_ParseManifest:PROC
EXTERNDEF Marketplace_ParseVsixHeader:PROC
EXTERNDEF Marketplace_RegisterExtension:PROC
EXTERNDEF Marketplace_ResolveDependencies:PROC
EXTERNDEF Observable_Create_ActiveTextEditor:PROC
EXTERNDEF Observable_Create_VisibleTextEditors:PROC
EXTERNDEF Observable_Create_WorkspaceFolders:PROC
EXTERNDEF OutputChannel_Append:PROC
EXTERNDEF OutputChannel_AppendLine:PROC
EXTERNDEF OutputChannel_Create:PROC
EXTERNDEF OutputChannel_CreateAPI:PROC
EXTERNDEF Provider_FromDocumentSelector:PROC
EXTERNDEF Provider_Register:PROC
EXTERNDEF WebviewPanel_CreateAPI:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; Phase System / Orchestration
; Source: Phase1_Master.asm, Phase2_Master.asm, Phase5_Master_Complete.asm
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF Phase_AgentKernel_Init:PROC
EXTERNDEF Phase_AgentKernel_Shutdown:PROC
EXTERNDEF Phase_Hardware_Init:PROC
EXTERNDEF Phase_Hardware_Shutdown:PROC
EXTERNDEF Phase_Inference_Init:PROC
EXTERNDEF Phase_Inference_Shutdown:PROC
EXTERNDEF Phase_InitializeAll:PROC
EXTERNDEF Phase_ModelLoading_Init:PROC
EXTERNDEF Phase_ModelLoading_Shutdown:PROC
EXTERNDEF Phase_Orchestration_Init:PROC
EXTERNDEF Phase_Orchestration_Shutdown:PROC
EXTERNDEF Phase_SwarmIO_Init:PROC
EXTERNDEF Phase_SwarmIO_Shutdown:PROC
EXTERNDEF Phase_UI_Init:PROC
EXTERNDEF Phase_UI_Shutdown:PROC
EXTERNDEF Phase_Week1_Init:PROC
EXTERNDEF Phase_Week1_Shutdown:PROC
EXTERNDEF Phase_Week5_Init:PROC
EXTERNDEF Phase_Week5_Shutdown:PROC
EXTERNDEF Phase_Weeks2_3_Init:PROC
EXTERNDEF Phase_Weeks2_3_Shutdown:PROC
EXTERNDEF Phase1Initialize:PROC
EXTERNDEF Phase2Initialize:PROC
EXTERNDEF Phase3Initialize:PROC
EXTERNDEF Phase3Shutdown:PROC
EXTERNDEF LogPhaseError:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; Swarm / Distributed
; Source: RawrXD_Swarm_Orchestrator.asm, WEEK1_COMPLETE.asm
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF Swarm_ExecuteAsync:PROC
EXTERNDEF Swarm_RunCoordinator:PROC
EXTERNDEF Swarm_Shutdown:PROC
EXTERNDEF Swarm_AllReduce_SUM:PROC
EXTERNDEF Swarm_SubmitJob:PROC
EXTERNDEF Swarm_WorkerThread:PROC
EXTERNDEF State_LoadEmergencyCheckpoint:PROC
EXTERNDEF State_SaveEmergencyCheckpoint:PROC

; ─── Consensus Protocols ──────────────────────────────────────────────────────
EXTERNDEF RaftMainLoop:PROC
EXTERNDEF Raft_AppendEntries:PROC
EXTERNDEF Raft_RequestVote:PROC
EXTERNDEF BFT_CompareVotes:PROC
EXTERNDEF BFT_VerifyQuorum:PROC
EXTERNDEF GossipMainLoop:PROC
EXTERNDEF Gossip_Init:PROC
EXTERNDEF Gossip_Send:PROC
EXTERNDEF Gossip_ThreadProc:PROC

; ─── Heartbeat ────────────────────────────────────────────────────────────────
EXTERNDEF Heartbeat_AddNode:PROC
EXTERNDEF Heartbeat_Initialize:PROC
EXTERNDEF Heartbeat_Shutdown:PROC
EXTERNDEF HeartbeatBatchUpdateHidden:PROC
EXTERNDEF HeartbeatMonitorThread:PROC
EXTERNDEF HeartbeatRecvThread:PROC
EXTERNDEF HeartbeatSendThread:PROC
EXTERNDEF HeartbeatThreadProc:PROC
EXTERNDEF ProcessReceivedHeartbeat:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD Core
; Source: various RawrXD_*.asm files
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF RawrXD_Blind_Sensors:PROC
EXTERNDEF RawrXD_Calculate_Jitter:PROC
EXTERNDEF RawrXD_Camellia_256_Decrypt_Block:PROC
EXTERNDEF RawrXD_CancelOperation:PROC
EXTERNDEF RawrXD_ChatServer_Shutdown:PROC
EXTERNDEF RawrXD_Check_DebugPort:PROC
EXTERNDEF RawrXD_Detokenize:PROC
EXTERNDEF RawrXD_ExceptionFilter:PROC
EXTERNDEF RawrXD_Execute_Task:PROC
EXTERNDEF RawrXD_Find_Gadgets:PROC
EXTERNDEF RawrXD_Generate_Junk:PROC
EXTERNDEF RawrXD_GetMetrics:PROC
EXTERNDEF RawrXD_GetPerformance:PROC
EXTERNDEF RawrXD_GetStats:PROC
EXTERNDEF RawrXD_Inference:PROC
EXTERNDEF RawrXD_InitializeAll:PROC
EXTERNDEF RawrXD_Initialize_AllPhases:PROC
EXTERNDEF RawrXD_InitUI:PROC
EXTERNDEF RawrXD_Integrate_To_Stream:PROC
EXTERNDEF RawrXD_Main_Entry:PROC
EXTERNDEF RawrXD_Receive_Task:PROC
EXTERNDEF RawrXD_RegisterDragDrop:PROC
EXTERNDEF RawrXD_Send_Heartbeat:PROC
EXTERNDEF RawrXD_ShowUploaderDialog:PROC
EXTERNDEF RawrXD_ShutdownAll:PROC
EXTERNDEF RawrXD_ShutdownUI:PROC
EXTERNDEF RawrXD_Socket_Connect:PROC
EXTERNDEF RawrXD_Socket_Init:PROC
EXTERNDEF RawrXD_SubmitChatRequest:PROC
EXTERNDEF RawrXD_Timing_Check:PROC
EXTERNDEF RawrXD_Tokenize:PROC
EXTERNDEF RawrXD_Tokenize_SSE42:PROC
EXTERNDEF RawrXD_UnloadModel:PROC
EXTERNDEF RawrXD_VectorizedExceptionHandler:PROC
EXTERNDEF RawrXD_WndProc:PROC
EXTERNDEF rawrxd_signal_patch_start:PROC
EXTERNDEF rawrxd_signal_patch_end:PROC
EXTERNDEF rawrxd_stream_enqueue_chunk:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; JSON Utilities
; Source: RawrXD_JSON_Parser.asm
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF JsonObject_Create:PROC
EXTERNDEF JsonParser_Parse:PROC
EXTERNDEF Json_EscapeString:PROC
EXTERNDEF Json_GetArray:PROC
EXTERNDEF Json_GetArrayField:PROC
EXTERNDEF Json_GetInt:PROC
EXTERNDEF Json_GetObjectField:PROC
EXTERNDEF Json_GetObjectKeys:PROC
EXTERNDEF Json_GetString:PROC
EXTERNDEF Json_GetStringField:PROC
EXTERNDEF Json_HasField:PROC
EXTERNDEF Json_ParseFast:PROC
EXTERNDEF Json_ParseFile:PROC
EXTERNDEF Json_ParseObject:PROC
EXTERNDEF Json_ParseString:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; HashMap / Collections
; Source: RawrXD_Complete_Interconnect.asm or similar
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF ArrayList_Add:PROC
EXTERNDEF ArrayList_Clear:PROC
EXTERNDEF ArrayList_Create:PROC
EXTERNDEF HashMapLookup:PROC
EXTERNDEF HashMap_Create:PROC
EXTERNDEF HashMap_ForEach:PROC
EXTERNDEF HashMap_Get:PROC
EXTERNDEF HashMap_Put:PROC
EXTERNDEF HashMap_Remove:PROC
EXTERNDEF DependencyGraph_AddNode:PROC
EXTERNDEF DependencyGraph_Create:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; PE Analysis / Reverse Engineering
; Source: dumpbin_final.asm, binary_analyzer_final.asm, RawrXD_MetaReverse.asm
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF AnalyzeControlFlow:PROC
EXTERNDEF AnalyzeFileHeader:PROC
EXTERNDEF AnalyzeLoadConfig:PROC
EXTERNDEF AnalyzeMemoryLayout:PROC
EXTERNDEF AnalyzeOptionalHeader:PROC
EXTERNDEF AnalyzeReloc:PROC
EXTERNDEF AnalyzeRelocations:PROC
EXTERNDEF AnalyzeResources:PROC
EXTERNDEF AnalyzeRichHeader:PROC
EXTERNDEF BinaryDiff:PROC
EXTERNDEF CalcEntropy:PROC
EXTERNDEF CalculateAdler32:PROC
EXTERNDEF CalculateCRC32:PROC
EXTERNDEF CompareFiles:PROC
EXTERNDEF ComputeFileHash:PROC
EXTERNDEF DeepAnalysis:PROC
EXTERNDEF DetectEncryption:PROC
EXTERNDEF DetectPacker:PROC
EXTERNDEF DisassembleInstruction:PROC
EXTERNDEF DisassemblerInit:PROC
EXTERNDEF DoCertificate:PROC
EXTERNDEF DoMemory:PROC
EXTERNDEF DoYARA:PROC
EXTERNDEF DumpDOSHeader:PROC
EXTERNDEF DumpMetadata:PROC
EXTERNDEF DumpOptionalHeader:PROC
EXTERNDEF DumpPE:PROC
EXTERNDEF DumpPEFileHeader:PROC
EXTERNDEF DumpSectionHeaders:PROC
EXTERNDEF EnumExports:PROC
EXTERNDEF EnumImports:PROC
EXTERNDEF ExtractExports:PROC
EXTERNDEF ExtractResourceToDisk:PROC
EXTERNDEF FullAnalysis:PROC
EXTERNDEF FullReconstruct:PROC
EXTERNDEF GenerateFullReport:PROC
EXTERNDEF GeneratePE:PROC
EXTERNDEF Heuristic_Scan:PROC
EXTERNDEF InitPE:PROC
EXTERNDEF ParsePE:PROC
EXTERNDEF ReconExports:PROC
EXTERNDEF ReconImports:PROC
EXTERNDEF ReconstructSource:PROC
EXTERNDEF ScanEmbeddedFormats:PROC
EXTERNDEF ShowDOS:PROC
EXTERNDEF ShowExports:PROC
EXTERNDEF ShowFileHdr:PROC
EXTERNDEF ShowImports:PROC
EXTERNDEF ShowOptHdr:PROC
EXTERNDEF ShowSections:PROC
EXTERNDEF Validate_PE_Header:PROC
EXTERNDEF ValidatePE:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; Crypto / Security
; Source: RawrXD_Complete_Interconnect.asm, etc.
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF ApplyCompileTimeObfuscation:PROC
EXTERNDEF Camellia_Decrypt:PROC
EXTERNDEF CRC32_Finalize:PROC
EXTERNDEF CRC32_Init:PROC
EXTERNDEF CRC32_Update:PROC
EXTERNDEF GenerateGhostKeyPair:PROC
EXTERNDEF RS_Decode:PROC
EXTERNDEF RS_Encode:PROC
EXTERNDEF RS_Init:PROC
EXTERNDEF SHA256_Hash:PROC
EXTERNDEF SeedXorEntropy:PROC
EXTERNDEF QuantumAuth_VerifyFingerprint:PROC
EXTERNDEF QuantumAuth_VerifyModelSignature:PROC
EXTERNDEF QuantumBufferCreate:PROC
EXTERNDEF QuantumEntangle:PROC

; ─── Anti-Debug / Anti-VM ─────────────────────────────────────────────────────
EXTERNDEF Check_IsDebuggerPresent:PROC
EXTERNDEF Check_RemoteDebuggerPresent:PROC
EXTERNDEF RawrXD_Timing_Check:PROC
EXTERNDEF RawrXD_Check_DebugPort:PROC
EXTERNDEF InstallHardwareBreakpoint:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; Telemetry / Observability
; Source: telemetry modules
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF API_IsTelemetryEnabled:PROC
EXTERNDEF Telemetry_BuildJSONPayload:PROC
EXTERNDEF Telemetry_FlushThread:PROC
EXTERNDEF Telemetry_Init:PROC
EXTERNDEF Telemetry_LogCrash:PROC
EXTERNDEF Telemetry_LogError:PROC
EXTERNDEF Telemetry_LogEvent:PROC
EXTERNDEF Telemetry_Record:PROC
EXTERNDEF Telemetry_SanitizeData:PROC
EXTERNDEF Telemetry_Shutdown:PROC
EXTERNDEF RecordTelemetryEvent:PROC

; ─── Prometheus Metrics ───────────────────────────────────────────────────────
EXTERNDEF Prometheus_AcceptThread:PROC
EXTERNDEF Prometheus_BuildMetrics:PROC
EXTERNDEF Prometheus_HandleClient:PROC
EXTERNDEF ExportMetrics:PROC

; ─── Profiler ─────────────────────────────────────────────────────────────────
EXTERNDEF Profiler_GetReport:PROC
EXTERNDEF Profiler_Init:PROC
EXTERNDEF Profiler_RecordAllocation:PROC
EXTERNDEF Profiler_RecordFunctionEnter:PROC
EXTERNDEF Profiler_RecordFunctionExit:PROC
EXTERNDEF Profiler_Start:PROC
EXTERNDEF Profiler_Stop:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; Crash Handling / Diagnostics
; Source: exception/SEH_wrapper.asm, various
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF CrashHandler_CreateMinidump:PROC
EXTERNDEF CrashHandler_ExceptionFilter:PROC
EXTERNDEF CrashHandler_Install:PROC
EXTERNDEF CrashHandler_ShutdownGracefully:PROC
EXTERNDEF Diagnose_BlockedFeatures:PROC
EXTERNDEF Validate_BlockedFeatures:PROC
EXTERNDEF Initialize_BlockedFeatures:PROC
EXTERNDEF Compiler_InitializeBlockedFeatures:PROC
EXTERNDEF ExceptionHandler:PROC
EXTERNDEF ExceptionFilterCallback:PROC
EXTERNDEF VectoredExceptionHandler:PROC
EXTERNDEF YTFN_ExceptionHandler:PROC
EXTERNDEF KernelEntry_SEH:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; LSP / IntelliSense / Language Server
; Source: RawrXD_LSP_Core.asm, RawrXD_LSP_Handshake_Ext.asm
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF DiagnosticsInit:PROC
EXTERNDEF DiagnosticsRenderSquiggles:PROC
EXTERNDEF Diagnostic_Add:PROC
EXTERNDEF IntelliSenseInit:PROC
EXTERNDEF IntelliSenseApplyCompletion:PROC
EXTERNDEF IntelliSenseRenderPopup:PROC
EXTERNDEF LSPParseCapabilities:PROC
EXTERNDEF LSPSendInitialized:PROC
EXTERNDEF LSP_Init_Handshake:PROC
EXTERNDEF LSP_Handshake_CheckTimeout:PROC
EXTERNDEF LSP_Handshake_Create:PROC
EXTERNDEF LSP_Handshake_Destroy:PROC
EXTERNDEF LSP_Handshake_DetectServer:PROC
EXTERNDEF LSP_MessageThread:PROC
EXTERNDEF LSPClient_CodeAction:PROC
EXTERNDEF LSPClient_Completion:PROC
EXTERNDEF LSPClient_FindReferences:PROC
EXTERNDEF LSPClient_GotoDefinition:PROC
EXTERNDEF LSPClient_Hover:PROC
EXTERNDEF LSPClient_Initialize:PROC
EXTERNDEF LSPClient_Rename:PROC
EXTERNDEF LSPCompletion:PROC
EXTERNDEF LSPTextDocumentDidChange:PROC
EXTERNDEF LSPTextDocumentDidOpen:PROC
EXTERNDEF LspClient_ForwardMessage:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; Config / Registry / Settings
; Source: config modules, registry_core
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF Config_CreateDefault:PROC
EXTERNDEF Config_Decrypt:PROC
EXTERNDEF Config_Encrypt:PROC
EXTERNDEF Config_EncryptSensitiveFields:PROC
EXTERNDEF Config_Load:PROC
EXTERNDEF Config_Save:PROC
EXTERNDEF Config_SerializeToJSON:PROC
EXTERNDEF Registry_CreateKey:PROC
EXTERNDEF Registry_KeyExists:PROC
EXTERNDEF Registry_SetDwordValue:PROC
EXTERNDEF Registry_SetQwordValue:PROC
EXTERNDEF Registry_SetStringValue:PROC
EXTERNDEF SettingsManager_Get:PROC
EXTERNDEF SettingsManager_Init:PROC
EXTERNDEF SettingsManager_Load:PROC
EXTERNDEF SettingsManager_Set:PROC

; ─── SemVer ───────────────────────────────────────────────────────────────────
EXTERNDEF SemVer_Parse:PROC
EXTERNDEF SemVer_ParseRange:PROC
EXTERNDEF SemVer_Satisfies:PROC
EXTERNDEF Path_Join:PROC
EXTERNDEF Path_Join_PackageJson:PROC
EXTERNDEF String_Join_Dot:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; Network / HTTP / Chat Server
; Source: RawrXD_HttpChatServer.asm, RawrXD_NativeHttpServer.asm
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF Network_Initialize:PROC
EXTERNDEF Network_Shutdown:PROC
EXTERNDEF HandleChatRequest:PROC
EXTERNDEF HttpRouter_ListenThread:PROC
EXTERNDEF HttpServer_GetStatus:PROC
EXTERNDEF HttpServer_Initialize:PROC
EXTERNDEF HttpServer_IsRunning:PROC
EXTERNDEF HttpServer_LoadModel:PROC
EXTERNDEF HttpServer_Shutdown:PROC
EXTERNDEF ProxyToOllama:PROC
EXTERNDEF SendChatMessage:PROC
EXTERNDEF StopChatServer:PROC
EXTERNDEF StreamErrorMessage:PROC
EXTERNDEF StreamSuccessMessage:PROC
EXTERNDEF StreamThreadProc:PROC
EXTERNDEF StreamToPowerShell:PROC
EXTERNDEF ExtractUrlPath:PROC
EXTERNDEF FormatToolResponse:PROC
EXTERNDEF Router_GetRoute:PROC
EXTERNDEF Router_GetStats:PROC
EXTERNDEF Router_Init:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; Codebase Context / Workspace
; Source: various IDE modules
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF CodebaseContext_FindSymbol:PROC
EXTERNDEF CodebaseContext_GetDefinitions:PROC
EXTERNDEF CodebaseContext_IndexFile:PROC
EXTERNDEF CodebaseContext_Init:PROC
EXTERNDEF Workspace_Create:PROC
EXTERNDEF Workspace_ScanFiles:PROC
EXTERNDEF Workspace_WatchThread:PROC
EXTERNDEF WorkspaceEdit_Apply:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; CoT / Predictive Engine
; Source: chain_of_thought modules
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF CoT_Initialize:PROC
EXTERNDEF CoT_Shutdown:PROC
EXTERNDEF PredictiveEngineInit:PROC
EXTERNDEF PredictiveWarmCache:PROC
EXTERNDEF PredictiveShow:PROC
EXTERNDEF SpawnSemanticAnalysis:PROC
EXTERNDEF SemanticEnhanceTokens:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; Lexer / Tokenizer
; Source: RawrXD_Lexer_AVX2.asm, blocked_features_manifest.asm
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF FastRegexTokenize:PROC
EXTERNDEF Lexer_Blocked_AVX512_Advanced:PROC
EXTERNDEF Lexer_Blocked_NeuralNetwork:PROC
EXTERNDEF Lexer_Blocked_QuantumInspired:PROC
EXTERNDEF Lexer_Destroy:PROC
EXTERNDEF Lexer_GetPerformance:PROC
EXTERNDEF Lexer_Hidden_CodeAnalysis:PROC
EXTERNDEF Lexer_Hidden_DiagnosticAnalysis:PROC
EXTERNDEF Lexer_Hidden_OptimizationPass:PROC
EXTERNDEF Lexer_Hidden_PredictiveAnalysis:PROC
EXTERNDEF Lexer_Hidden_SelfModifyingCode:PROC
EXTERNDEF Lexer_InitKeywords:PROC
EXTERNDEF Lexer_Initialize:PROC
EXTERNDEF Lexer_ReadNumber:PROC
EXTERNDEF Lexer_ReadOperator:PROC
EXTERNDEF Lexer_ReadString:PROC
EXTERNDEF Lexer_Scan_Parallel:PROC
EXTERNDEF TokenizeLineIncremental:PROC
EXTERNDEF TokenizeText:PROC
EXTERNDEF TokenStream_Create:PROC
EXTERNDEF TokenStream_Read:PROC
EXTERNDEF TokenStream_Write:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; Text Editor / Piece Table / Buffer
; Source: Monaco/editor modules
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF BufferDelete:PROC
EXTERNDEF BufferInsert:PROC
EXTERNDEF CursorMoveLeft:PROC
EXTERNDEF CursorMoveRight:PROC
EXTERNDEF FindPieceAtOffset:PROC
EXTERNDEF InsertNewPiece:PROC
EXTERNDEF InvalidateLineCacheFrom:PROC
EXTERNDEF QuantumPieceAlloc:PROC
EXTERNDEF RebalanceTree:PROC
EXTERNDEF SplitPiece:PROC
EXTERNDEF Undo:PROC
EXTERNDEF ApplyEdit:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; Thread Pool / Scheduler / Synchronization
; Source: RawrXD_System_Primitives.asm, etc.
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF SpinLock_Acquire:PROC
EXTERNDEF SpinLock_Release:PROC
EXTERNDEF RWLock_AcquireRead:PROC
EXTERNDEF RWLock_AcquireWrite:PROC
EXTERNDEF RWLock_ReleaseRead:PROC
EXTERNDEF RWLock_ReleaseWrite:PROC
EXTERNDEF ThreadPool_WaitComplete:PROC
EXTERNDEF ThreadPool_WorkerProc:PROC
EXTERNDEF WorkerThreadProc:PROC
EXTERNDEF Worker_Thread_Proc:PROC
EXTERNDEF Scheduler_Initialize:PROC
EXTERNDEF Scheduler_Shutdown:PROC
EXTERNDEF Scheduler_SubmitTask:PROC
EXTERNDEF Scheduler_WaitForTask:PROC
EXTERNDEF SchedulerCoordinatorThread:PROC
EXTERNDEF SchedulerThreadProc:PROC
EXTERNDEF SubmitTask:PROC
EXTERNDEF Aligned_Allocate:PROC
EXTERNDEF AtomicCompareExchange64:PROC
EXTERNDEF AtomicDecrement64:PROC
EXTERNDEF AtomicLoad64:PROC
EXTERNDEF AtomicStore64:PROC
EXTERNDEF LockFreeQueue_Dequeue:PROC
EXTERNDEF LockFreeQueue_Destroy:PROC
EXTERNDEF LockFreeQueue_Enqueue:PROC
EXTERNDEF LockFreeQueue_Init:PROC
EXTERNDEF Thread_AffinitySet:PROC
EXTERNDEF Thread_GetCurrentProcessor:PROC
EXTERNDEF Thread_SetAffinityValidated:PROC
EXTERNDEF Thread_SetIdealProcessor:PROC
EXTERNDEF Thread_SetQoS:PROC

; ─── Load Balancer ────────────────────────────────────────────────────────────
EXTERNDEF LoadBalancer_Create:PROC
EXTERNDEF LoadBalancer_Push:PROC
EXTERNDEF LoadBalancer_Steal:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; Thermal / NVMe / Direct IO
; Source: thermal/masm/*.asm, direct_io/*.asm
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF NVMe_AllocAlignedBuffer:PROC
EXTERNDEF NVMe_FreeAlignedBuffer:PROC
EXTERNDEF NVMe_GetCachedTemp:PROC
EXTERNDEF NVMe_GetCoolestDrive:PROC
EXTERNDEF NVMe_GetLastError:PROC
EXTERNDEF NVMe_PollAllDrives:PROC
EXTERNDEF NVMe_StressBurst:PROC
EXTERNDEF QueryNVMeTemp:PROC
EXTERNDEF ThermalGovernor_ReadTemperature:PROC
EXTERNDEF OverclockGovernor_SetFrequency:PROC
EXTERNDEF Governor_HotSwap:PROC
EXTERNDEF PocketLabGetStats:PROC
EXTERNDEF PocketLabGetThermal:PROC
EXTERNDEF PocketLabRunCycle:PROC

; ─── Ghost Paging ─────────────────────────────────────────────────────────────
EXTERNDEF GhostDispatchToken:PROC
EXTERNDEF GhostPagingGetStats:PROC
EXTERNDEF GhostPagingPreloadCritical:PROC
EXTERNDEF GhostPagingResetReuse:PROC
EXTERNDEF GhostPagingSwapBuffer:PROC
EXTERNDEF GhostPrefetchStart:PROC
EXTERNDEF GhostPrefetchTokenMap:PROC
EXTERNDEF GhostQueryTokenMap:PROC
EXTERNDEF GhostUpdate:PROC
EXTERNDEF Ghost_C2_Handshake:PROC

; ─── JIT LBA / Burst ──────────────────────────────────────────────────────────
EXTERNDEF JitLBA_BurstTensor:PROC
EXTERNDEF JitLBA_HashTensorName:PROC
EXTERNDEF JitLBA_Init:PROC
EXTERNDEF JitLBA_SetDriveMMIO:PROC
EXTERNDEF DirectStorage_EnqueueRequest:PROC
EXTERNDEF DirectStorage_FlushQueue:PROC

; ─── Bounce/HotPatch/Sloloris/Unbraid/DirLoad TPS ────────────────────────────
EXTERNDEF Bounce_AttachModel:PROC
EXTERNDEF Bounce_Destroy:PROC
EXTERNDEF Bounce_GetStats:PROC
EXTERNDEF Bounce_GetTPS:PROC
EXTERNDEF Bounce_GetTensor:PROC
EXTERNDEF Bounce_Init:PROC
EXTERNDEF Bounce_NotifyTokenGen:PROC
EXTERNDEF Bounce_SetBounceRate:PROC
EXTERNDEF Bounce_SetHeatDecay:PROC
EXTERNDEF Bounce_SetPoolRatio:PROC
EXTERNDEF Bounce_SetPrefetchDepth:PROC
EXTERNDEF Bounce_SetTargetTPS:PROC
EXTERNDEF Bridge_Destroy:PROC
EXTERNDEF Bridge_GetStats:PROC
EXTERNDEF Bridge_GetStatus:PROC
EXTERNDEF Bridge_GetStrength:PROC
EXTERNDEF Bridge_GetTPS:PROC
EXTERNDEF Bridge_Init:PROC
EXTERNDEF Bridge_NotifyToken:PROC
EXTERNDEF Bridge_Tick:PROC
EXTERNDEF DirLoad_Destroy:PROC
EXTERNDEF DirLoad_FeedSlot:PROC
EXTERNDEF DirLoad_GetDirection:PROC
EXTERNDEF DirLoad_GetLoadPct:PROC
EXTERNDEF DirLoad_GetStats:PROC
EXTERNDEF DirLoad_HotPatch:PROC
EXTERNDEF DirLoad_Init:PROC
EXTERNDEF DirLoad_LockLoad:PROC
EXTERNDEF DirLoad_MemPatch:PROC
EXTERNDEF DirLoad_SetLoadPct:PROC
EXTERNDEF DirLoad_Tick:PROC
EXTERNDEF DirLoad_UnlockLoad:PROC
EXTERNDEF HotPatch_Destroy:PROC
EXTERNDEF HotPatch_GetMultiplier:PROC
EXTERNDEF HotPatch_GetParam:PROC
EXTERNDEF HotPatch_GetStats:PROC
EXTERNDEF HotPatch_GetTPS:PROC
EXTERNDEF HotPatch_Init:PROC
EXTERNDEF HotPatch_SetParam:PROC
EXTERNDEF Sloloris_AttachModel:PROC
EXTERNDEF Sloloris_Destroy:PROC
EXTERNDEF Sloloris_ForceOrbit:PROC
EXTERNDEF Sloloris_GetTensor:PROC
EXTERNDEF Sloloris_Init:PROC
EXTERNDEF Sloloris_SetStrategy:PROC
EXTERNDEF Unbraid_AddPressure:PROC
EXTERNDEF Unbraid_Destroy:PROC
EXTERNDEF Unbraid_Execute:PROC
EXTERNDEF Unbraid_ForceRebraid:PROC
EXTERNDEF Unbraid_ForceUnbraid:PROC
EXTERNDEF Unbraid_GetPressurePct:PROC
EXTERNDEF Unbraid_GetStats:PROC
EXTERNDEF Unbraid_Init:PROC
EXTERNDEF Unbraid_RegisterStage:PROC
EXTERNDEF Unbraid_ReleasePressure:PROC
EXTERNDEF Unbraid_SetBudget:PROC
EXTERNDEF Unbraid_Tick:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; INFINITY QuadBuffer / DMA
; Source: RawrXD_QuadBuffer_DMA_Orchestrator.asm
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF INFINITY_GetMetrics:PROC
EXTERNDEF INFINITY_GetSlotRamPtr:PROC
EXTERNDEF INFINITY_GetSlotState:PROC
EXTERNDEF INFINITY_GetSlotVramPtr:PROC
EXTERNDEF INFINITY_IOThreadProc:PROC
EXTERNDEF INFINITY_ProcessIOCP:PROC
EXTERNDEF INFINITY_ResetMetrics:PROC
EXTERNDEF INFINITY_RotateBuffers:PROC
EXTERNDEF INFINITY_TriggerPrefetch:PROC
EXTERNDEF QuadBuffer_CopyToGPU:PROC
EXTERNDEF QuadBuffer_IOCPWorker:PROC
EXTERNDEF QuadBuffer_InitializeStream:PROC
EXTERNDEF BENCHMARK_QUADBUFFER:PROC
EXTERNDEF FreeDMABuffer:PROC

; ─── Ring Buffer ──────────────────────────────────────────────────────────────
EXTERNDEF RingBufferConsumer_Shutdown:PROC
EXTERNDEF RingBufferThreadProc:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; OS Interceptor / Hooks
; Source: os_explorer_interceptor.asm
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF InitOSInterceptor:PROC
EXTERNDEF StartInterceptor:PROC
EXTERNDEF StopInterceptor:PROC
EXTERNDEF UnloadInterceptor:PROC
EXTERNDEF InjectInterceptor:PROC
EXTERNDEF InstallHook:PROC
EXTERNDEF InstallOSHooks:PROC
EXTERNDEF GenericHook:PROC
EXTERNDEF ExportCapturedData:PROC
EXTERNDEF GetHookTypeName:PROC
EXTERNDEF MyCreateFileWHook:PROC
EXTERNDEF MyDeleteFileHook:PROC
EXTERNDEF MyGetAddrInfoHook:PROC
EXTERNDEF MyMoveFileHook:PROC
EXTERNDEF MyReadFileHook:PROC
EXTERNDEF MyRecvHook:PROC
EXTERNDEF MyRegCreateKeyExWHook:PROC
EXTERNDEF MyRegDeleteKeyHook:PROC
EXTERNDEF MyRegOpenKeyExWHook:PROC
EXTERNDEF MyRegQueryValueExWHook:PROC
EXTERNDEF MyRegSetValueExWHook:PROC
EXTERNDEF MySendHook:PROC
EXTERNDEF MyWSAConnectHook:PROC
EXTERNDEF MyWriteFileHook:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; IPC / Pipe Server
; Source: RawrXD_PipeServer.asm, RawrXD_IPC_Bridge.asm
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF PipeIPC_Connect:PROC
EXTERNDEF PipeIPC_CreateServer:PROC
EXTERNDEF PipeIPC_Send:PROC
EXTERNDEF PipeIPC_ServerThread:PROC
EXTERNDEF StopPipeServer:PROC
EXTERNDEF IPC_AttachGPU:PROC
EXTERNDEF IPC_DetachGPU:PROC
EXTERNDEF IPC_ShareVulkanContext:PROC
EXTERNDEF IPC_WaitForMessage:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; MCP Integration
; Source: mcp_hooks.asm
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF MCP_ExtractMethod:PROC
EXTERNDEF MCP_HexDumpBuffer:PROC
EXTERNDEF MCP_OnSocketData_Trampoline:PROC
EXTERNDEF MCP_ParseContentLength:PROC
EXTERNDEF MCP_ReadMessage_Trampoline:PROC
EXTERNDEF MCP_WebSocketFrame_Trampoline:PROC
EXTERNDEF MCP_WriteMessage_Trampoline:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; Polymorphic Engine
; Source: masm_solo_compiler.asm
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF Polymorph_CFFlatten:PROC
EXTERNDEF Polymorph_InsertJunk:PROC
EXTERNDEF Polymorph_LoadStub:PROC
EXTERNDEF Polymorph_RandomizeRegisters:PROC
EXTERNDEF Polymorph_SelfHeal:PROC
EXTERNDEF Polymorph_ShuffleSections:PROC
EXTERNDEF Polymorph_Virtualize:PROC
EXTERNDEF ClassifyPattern:PROC
EXTERNDEF InitializePatternEngine:PROC
EXTERNDEF ShutdownPatternEngine:PROC
EXTERNDEF GetPatternStats:PROC
EXTERNDEF PatternDB_Init:PROC
EXTERNDEF PatternDB_Scan:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; UI / Window
; Source: Monaco modules, RawrXD_GUI_IDE.asm
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF MainWindow_OnFileOpen:PROC
EXTERNDEF MainWindow_PopulateModelInfo:PROC
EXTERNDEF MainWindow_UpdateTitleBar:PROC
EXTERNDEF ResolveModeHandler:PROC
EXTERNDEF EnterpriseEditorCreate:PROC
EXTERNDEF GlyphAtlas_GetMetrics:PROC
EXTERNDEF GlyphAtlas_RenderText:PROC
EXTERNDEF UIPanel_Create:PROC
EXTERNDEF UIPanel_Destroy:PROC
EXTERNDEF UIPanel_SetContent:PROC
EXTERNDEF UIPanel_Show:PROC
EXTERNDEF ViewCreate:PROC
EXTERNDEF ViewModelUpdate:PROC
EXTERNDEF ViewRender:PROC
EXTERNDEF ViewRenderLine:PROC
EXTERNDEF WndProc:PROC
EXTERNDEF WndProc_Command:PROC
EXTERNDEF WndProc_Editor:PROC
EXTERNDEF WndProc_Main:PROC
EXTERNDEF WindowProc:PROC
EXTERNDEF RealWndProc:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; Backup / Error Recovery
; Source: backup modules
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF BackupManager_CreateSnapshot:PROC
EXTERNDEF BackupManager_DeleteSnapshot:PROC
EXTERNDEF BackupManager_ListSnapshots:PROC
EXTERNDEF BackupManager_RestoreSnapshot:PROC
EXTERNDEF Error_GetMessage:PROC
EXTERNDEF Error_SetContext:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; Week 1 Foundation
; Source: WEEK1_COMPLETE.asm, WEEK1_DELIVERABLE.asm
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF Week1GetStatistics:PROC
EXTERNDEF Week1GetVersion:PROC
EXTERNDEF Week1Initialize:PROC
EXTERNDEF Week1RegisterNode:PROC
EXTERNDEF Week1RegisterResource:PROC
EXTERNDEF Week1Shutdown:PROC
EXTERNDEF Week1StartBackgroundThreads:PROC
EXTERNDEF Week1_ConflictDetectionThread:PROC
EXTERNDEF Week1_HeartbeatThread:PROC
EXTERNDEF Week1_SubmitTask:PROC
EXTERNDEF Week1_WorkerThread:PROC

; ─── Conflict Detection ──────────────────────────────────────────────────────
EXTERNDEF ConflictDetectorThread:PROC
EXTERNDEF ConflictDetector_Initialize:PROC
EXTERNDEF ConflictDetector_LockResource:PROC
EXTERNDEF ConflictDetector_RegisterResource:PROC
EXTERNDEF ConflictDetector_UnlockResource:PROC
EXTERNDEF ConflictThreadProc:PROC
EXTERNDEF DeadlockDetectionThread:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; String / Memory Utilities
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF CompareWideAscii:PROC
EXTERNDEF MemCpy_Optimized:PROC
EXTERNDEF Memory_PrefetchRead:PROC
EXTERNDEF PrefetchHidden:PROC
EXTERNDEF SafeFloatOp:PROC
EXTERNDEF SafeMemCopy:PROC
EXTERNDEF StrCmp_ASCII:PROC
EXTERNDEF Str_Compare:PROC
EXTERNDEF Str_Copy:PROC
EXTERNDEF Str_Length:PROC
EXTERNDEF TrimString:PROC
EXTERNDEF ValidatePointer:PROC
EXTERNDEF WideToAscii:PROC
EXTERNDEF SmartPtr_AddRef:PROC
EXTERNDEF SmartPtr_Create:PROC
EXTERNDEF SmartPtr_Destroy:PROC
EXTERNDEF __memcpy:PROC
EXTERNDEF __strcpy_w:PROC
EXTERNDEF __utf16_to_utf8:PROC
EXTERNDEF __utf8_to_utf16:PROC
EXTERNDEF __wcslen:PROC
EXTERNDEF RemovePadding:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; Timing / Performance Counters
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF QPCToMs:PROC
EXTERNDEF ReadTsc:PROC
EXTERNDEF StartPerformanceCounter:PROC
EXTERNDEF StopPerformanceCounter:PROC
EXTERNDEF Timing_GetElapsedMicros:PROC
EXTERNDEF Timing_GetTSC:PROC
EXTERNDEF TSCToNanosecondsHidden:PROC
EXTERNDEF GetNanosecondTimestampHidden:PROC
EXTERNDEF GetCurrentNumaNodeHidden:PROC

; ═══════════════════════════════════════════════════════════════════════════════
; Misc Internal
; ═══════════════════════════════════════════════════════════════════════════════
EXTERNDEF Assemble:PROC
EXTERNDEF AssertFailed:PROC
EXTERNDEF Auditor_Shutdown:PROC
EXTERNDEF DetectLang:PROC
EXTERNDEF DisplayMessage:PROC
EXTERNDEF DisplayMessageWithPid:PROC
EXTERNDEF ExecuteTest:PROC
EXTERNDEF Fmt:PROC
EXTERNDEF GenerateCMake:PROC
EXTERNDEF GenerateHeaderFile:PROC
EXTERNDEF GetEngineInfo:PROC
EXTERNDEF GetFileName:PROC
EXTERNDEF GetModelInfo:PROC
EXTERNDEF GetOptimalDrive:PROC
EXTERNDEF GitBlame:PROC
EXTERNDEF GitDiff:PROC
EXTERNDEF GitGetStatus:PROC
EXTERNDEF GitInit:PROC
EXTERNDEF GetHex:PROC
EXTERNDEF GetInt:PROC
EXTERNDEF InitCommandTable:PROC
EXTERNDEF Main:PROC
EXTERNDEF PrintChars:PROC
EXTERNDEF PrintDirectories:PROC
EXTERNDEF ProcessCLICommand:PROC
EXTERNDEF ProcessCommand:PROC
EXTERNDEF ProcessDirectory:PROC
EXTERNDEF PublicProc:PROC
EXTERNDEF RegisterResource:PROC
EXTERNDEF RegisterTool:PROC
EXTERNDEF ShowHelp:PROC
EXTERNDEF ShowStats:PROC
EXTERNDEF ShowStatus:PROC
EXTERNDEF ShutdownSystems:PROC
EXTERNDEF TemporalLogCreate:PROC
EXTERNDEF TemporalBranch:PROC
EXTERNDEF TemporalMerge:PROC
EXTERNDEF TimeTravelDebug:PROC
EXTERNDEF ToolExecuteJson:PROC
EXTERNDEF TestEntry:PROC
EXTERNDEF CPUID_Execute:PROC
EXTERNDEF IsDeviceAddress:PROC
EXTERNDEF IsX64Process:PROC

; ─── Logging ──────────────────────────────────────────────────────────────────
EXTERNDEF ClearLog:PROC
EXTERNDEF FormatLogEntry:PROC
EXTERNDEF Log_FlushThread:PROC
EXTERNDEF Log_Write:PROC
EXTERNDEF Robust_Log:PROC

; ─── Compiler Engine ──────────────────────────────────────────────────────────
EXTERNDEF AsmCompiler_Assemble:PROC
EXTERNDEF CompilerEngine_CompileSync:PROC
EXTERNDEF CompilerEngine_Destroy:PROC
EXTERNDEF CompilerEngine_ValidateOptions:PROC
EXTERNDEF CppCompiler_Compile:PROC
EXTERNDEF PythonCompiler_Compile:PROC
EXTERNDEF RustCompiler_Compile:PROC
EXTERNDEF Mode_Handler_Compile:PROC
EXTERNDEF Mode_Handler_Encrypt:PROC
EXTERNDEF Mode_Handler_Stubgen:PROC
EXTERNDEF TerraFormer_EmitBinary:PROC

; ─── File Operations ──────────────────────────────────────────────────────────
EXTERNDEF Directory_Create:PROC
EXTERNDEF File_ReadAllText:PROC
EXTERNDEF File_WriteAllText:PROC
EXTERNDEF FileHandle_Create:PROC
EXTERNDEF FileHandle_Destroy:PROC
EXTERNDEF FileHandle_ValidateAndClose:PROC

; ─── Cache ────────────────────────────────────────────────────────────────────
EXTERNDEF Cache_Cleanup:PROC
EXTERNDEF Cache_Evict:PROC
EXTERNDEF Cache_MoveToFront:PROC
EXTERNDEF Cache_Store:PROC
EXTERNDEF L3CachePool_Cleanup:PROC

; ─── DLL / Service Entry ──────────────────────────────────────────────────────
EXTERNDEF DllMain:PROC
EXTERNDEF _DllMainCRTStartup:PROC
EXTERNDEF WinMainCRTStartup:PROC
EXTERNDEF mainCRTStartup:PROC
EXTERNDEF ServiceCtrl:PROC
EXTERNDEF ServiceEntry:PROC
EXTERNDEF ServiceHandler:PROC
EXTERNDEF ServiceMain:PROC
EXTERNDEF SidecarEntry:PROC
EXTERNDEF SidecarMinimalEntry:PROC
EXTERNDEF AgentBridgeEntry:PROC
EXTERNDEF OrchestratorInitialize:PROC
EXTERNDEF PEBMain:PROC

; ─── Sovereign Stress / Thermal Demo ──────────────────────────────────────────
EXTERNDEF SovereignStressMain:PROC
EXTERNDEF StressAdapt:PROC
EXTERNDEF BurstExecute:PROC

; ─── CLI ──────────────────────────────────────────────────────────────────────
EXTERNDEF CLIMainLoop:PROC
EXTERNDEF CLIThreadProc:PROC
EXTERNDEF CleanupCLI:PROC
EXTERNDEF CMD_Clear:PROC
EXTERNDEF CMD_Exit:PROC
EXTERNDEF CMD_Export:PROC
EXTERNDEF CMD_Filter:PROC
EXTERNDEF CMD_Help:PROC
EXTERNDEF CMD_Start:PROC
EXTERNDEF CMD_Stats:PROC
EXTERNDEF CMD_Status:PROC
EXTERNDEF CMD_Stop:PROC

; ─── Validation Suite ─────────────────────────────────────────────────────────
EXTERNDEF ValidateBUG:PROC
EXTERNDEF ValidateFIXME:PROC
EXTERNDEF ValidateHACK:PROC
EXTERNDEF ValidateIDEA:PROC
EXTERNDEF ValidateNOTE:PROC
EXTERNDEF ValidateREVIEW:PROC
EXTERNDEF ValidateTODO:PROC
EXTERNDEF ValidateXXX:PROC
EXTERNDEF RUN_VALIDATION_SUITE:PROC

; ─── Robust Tools ─────────────────────────────────────────────────────────────
EXTERNDEF Robust_AtomicCompareExchange:PROC
EXTERNDEF Robust_AtomicInc64:PROC
EXTERNDEF Robust_Crc64Init:PROC
EXTERNDEF Robust_Crc64Update:PROC
EXTERNDEF Robust_Free:PROC
EXTERNDEF Robust_Initialize:PROC
EXTERNDEF Robust_OpenStream:PROC
EXTERNDEF Robust_SkipString:PROC

; ─── Neon Fabric ──────────────────────────────────────────────────────────────
EXTERNDEF NeonFabricInitialize_ASM:PROC
EXTERNDEF NeonFabricShutdown_ASM:PROC

; ─── Hidden Subsystem Performance ─────────────────────────────────────────────
EXTERNDEF AdaptiveLockAcquireHidden:PROC
EXTERNDEF FlushCacheLineHidden:PROC
EXTERNDEF InitializeHiddenSubsystem:PROC
EXTERNDEF InterleavedLoadHidden:PROC
EXTERNDEF LFQueuePopHidden:PROC
EXTERNDEF LFQueuePushHidden:PROC
EXTERNDEF SubmitIOBatchHidden:PROC
EXTERNDEF WaitForGraphAddEdgeHidden:PROC
EXTERNDEF WaitForGraphRemoveEdgeHidden:PROC

; ─── ASN1 Certificate Patching ────────────────────────────────────────────────
EXTERNDEF ASN1_PatchField:PROC
EXTERNDEF ASN1_PatchSubject:PROC
EXTERNDEF ASN1_PatchValidity:PROC

; ─── SIMD / AVX512 ───────────────────────────────────────────────────────────
EXTERNDEF SIMD_Classify:PROC
EXTERNDEF avx512_scan_stubs:PROC

; ─── Autotune ─────────────────────────────────────────────────────────────────
EXTERNDEF Autotune_Create:PROC
EXTERNDEF Autotune_ReportScore:PROC
EXTERNDEF Autotune_Suggest:PROC

; ─── Injection ────────────────────────────────────────────────────────────────
EXTERNDEF InjectDll_CreateRemoteThread:PROC
EXTERNDEF InjectDll_NtCreateThreadEx:PROC
EXTERNDEF InjectDll_QueueUserAPC:PROC

; ─── DMA Assembly Helpers ─────────────────────────────────────────────────────
EXTERNDEF rxd_asm_calculate_diff:PROC
EXTERNDEF rxd_asm_cancel_token:PROC
EXTERNDEF rxd_asm_copy_to_dma:PROC
EXTERNDEF rxd_asm_create_cancel_token:PROC
EXTERNDEF rxd_asm_emit_metric:PROC
EXTERNDEF rxd_asm_is_cancelled:PROC
EXTERNDEF rxd_asm_normalize_completion:PROC
EXTERNDEF rxd_asm_scan_identifiers:PROC

; ─── Streaming Orchestrator ───────────────────────────────────────────────────
EXTERNDEF NativeInferenceThread:PROC
EXTERNDEF Vram_Free:PROC
EXTERNDEF Vram_SetCallbacks:PROC
EXTERNDEF Vram_SubmitUpload:PROC
EXTERNDEF StagingBuffer_Deallocate:PROC

; ─── Digestion Engine ─────────────────────────────────────────────────────────
EXTERNDEF RunDigestionEngine:PROC
EXTERNDEF DigestionFastScan:PROC
EXTERNDEF DigestionHashChunk:PROC
EXTERNDEF ScrubThread:PROC

; ─── Model Trainer ────────────────────────────────────────────────────────────
EXTERNDEF ModelTrainer_Train:PROC
EXTERNDEF Optimizer_AdamWStep:PROC

; ─── GRPC ─────────────────────────────────────────────────────────────────────
EXTERNDEF GRPC_Call:PROC

; ─── Update Checker ───────────────────────────────────────────────────────────
EXTERNDEF UpdateCheckerThread:PROC
EXTERNDEF CrashReportUploadThread:PROC
EXTERNDEF HealingWorkerThread:PROC

; ─── Overlay / Game ───────────────────────────────────────────────────────────
EXTERNDEF AROverlay:PROC
EXTERNDEF AimbotUpdate:PROC
EXTERNDEF BhopScroll:PROC
EXTERNDEF ChamsRender:PROC
EXTERNDEF ESPInit:PROC
EXTERNDEF ESPRenderGlow:PROC
EXTERNDEF ESPScanSymbols:PROC
EXTERNDEF EyeTrackScroll:PROC
EXTERNDEF HapticType:PROC
EXTERNDEF HeatMapToggle:PROC
EXTERNDEF HexEditToggle:PROC
EXTERNDEF HoloRenderFrame:PROC
EXTERNDEF MemoryEditAttach:PROC
EXTERNDEF Minimap3D:PROC
EXTERNDEF SemanticZoom:PROC
EXTERNDEF SensoryCode:PROC
EXTERNDEF SpeedhackSetScale:PROC
EXTERNDEF TriggerbotCheck:PROC
EXTERNDEF VoiceCommand:PROC
EXTERNDEF DNASequence:PROC
EXTERNDEF BCIPrep:PROC
EXTERNDEF DecompileView:PROC

; ─── Beacon / C2 ──────────────────────────────────────────────────────────────
EXTERNDEF Beacon_AntiForensics:PROC
EXTERNDEF Beacon_Exfiltrate:PROC
EXTERNDEF Beacon_InitC2:PROC
EXTERNDEF Beacon_MainLoop:PROC
EXTERNDEF Beacon_Persist:PROC

; ─── PWM / Throttle ──────────────────────────────────────────────────────────
EXTERNDEF applyPWMThrottle:PROC
EXTERNDEF clearEmergencyStop:PROC
EXTERNDEF dispatchLoop:PROC
EXTERNDEF dispatchOnce:PROC
EXTERNDEF fillRandomBuffer:PROC
EXTERNDEF getThrottleLevel:PROC
EXTERNDEF setThrottleLevel:PROC
EXTERNDEF triggerEmergencyStop:PROC

; ─── Entropy / Random ─────────────────────────────────────────────────────────
EXTERNDEF generateAndInstallKey:PROC
EXTERNDEF generateRandom32:PROC
EXTERNDEF generateRandom64:PROC
EXTERNDEF generateSeed64:PROC
EXTERNDEF hasRdrand:PROC
EXTERNDEF hasRdseed:PROC
EXTERNDEF testEntropyQuality:PROC
EXTERNDEF validateSessionKey:PROC

; ─── GPU Portable API ─────────────────────────────────────────────────────────
EXTERNDEF cuda_detect_device:PROC
EXTERNDEF cuda_execute_kernel:PROC
EXTERNDEF cuda_init:PROC
EXTERNDEF cuda_memory_alloc:PROC
EXTERNDEF cuda_shutdown:PROC
EXTERNDEF rocm_detect_device:PROC
EXTERNDEF rocm_execute_kernel:PROC
EXTERNDEF rocm_init:PROC
EXTERNDEF rocm_memory_alloc:PROC
EXTERNDEF vulkan_detect_device:PROC
EXTERNDEF vulkan_execute_command:PROC
EXTERNDEF vulkan_init:PROC
EXTERNDEF vulkan_memory_alloc:PROC
EXTERNDEF gpu_free:PROC
EXTERNDEF gpu_malloc:PROC
EXTERNDEF gpu_memcpy_device_to_host:PROC
EXTERNDEF gpu_memcpy_host_to_device:PROC
EXTERNDEF gpu_synchronize:PROC
EXTERNDEF launch_element_mul_kernel:PROC
EXTERNDEF launch_matmul_kernel:PROC
EXTERNDEF launch_vector_add_kernel:PROC
EXTERNDEF vk_init:PROC
EXTERNDEF vk_shutdown:PROC

; ─── Code Generation (compiler toolchain) ─────────────────────────────────────
EXTERNDEF emit_call_rel:PROC
EXTERNDEF emit_jcc_rel:PROC
EXTERNDEF emit_mov_rax_imm:PROC
EXTERNDEF emit_rexw_op:PROC
EXTERNDEF emit_word:PROC

; ─── Deflate ──────────────────────────────────────────────────────────────────
EXTERNDEF deflate_brutal_masm:PROC

; ─── Initialization Helpers ───────────────────────────────────────────────────
EXTERNDEF InitializeTensorEngine:PROC
EXTERNDEF TrackAlloc:PROC
EXTERNDEF TrackFree:PROC

ENDIF ; RAWRXD_INTERNAL_PROTOS_INC
