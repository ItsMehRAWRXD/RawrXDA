; RawrXD_StubSweepBridge.asm
; Auto-generated MASM bridge implementations for remaining stub-only bridge exports.

OPTION CASEMAP:NONE

EXTERN OutputDebugStringA:PROC
EXTERN malloc:PROC

includelib kernel32.lib

.data
sz_stub_0 db "[MASM] AccelRouter_Create",0
sz_stub_1 db "[MASM] AccelRouter_ForceBackend",0
sz_stub_2 db "[MASM] AccelRouter_GetActiveBackend",0
sz_stub_3 db "[MASM] AccelRouter_GetStatsJson",0
sz_stub_4 db "[MASM] AccelRouter_Init",0
sz_stub_5 db "[MASM] AccelRouter_IsBackendAvailable",0
sz_stub_6 db "[MASM] AccelRouter_Shutdown",0
sz_stub_7 db "[MASM] AccelRouter_Submit",0
sz_stub_8 db "[MASM] Acquire_CoT_Lock",0
sz_stub_9 db "[MASM] Acquire_CoT_Lock_Shared",0
sz_stub_10 db "[MASM] AgentRouter_ExecuteTask",0
sz_stub_11 db "[MASM] AgentRouter_Initialize",0
sz_stub_12 db "[MASM] AgentTool_QuantizeModel",0
sz_stub_13 db "[MASM] Apply_FFN_SwiGLU",0
sz_stub_14 db "[MASM] Apply_RMSNorm",0
sz_stub_15 db "[MASM] Apply_RoPE_Direct",0
sz_stub_16 db "[MASM] ArrayList_Add",0
sz_stub_17 db "[MASM] ArrayList_Clear",0
sz_stub_18 db "[MASM] ArrayList_Create",0
sz_stub_19 db "[MASM] asm_apply_memory_patch",0
sz_stub_20 db "[MASM] asm_camellia256_decrypt_ctr",0
sz_stub_21 db "[MASM] asm_camellia256_encrypt_ctr",0
sz_stub_22 db "[MASM] asm_camellia256_get_hmac_key",0
sz_stub_23 db "[MASM] CleanupInference",0
sz_stub_24 db "[MASM] CompletionProvider_Adapter_Create",0
sz_stub_25 db "[MASM] Compute_MHA_Parallel",0
sz_stub_26 db "[MASM] ConsolePrint",0
sz_stub_27 db "[MASM] CoT_EnableMultiProducer",0
sz_stub_28 db "[MASM] CoT_Has_Large_Pages",0
sz_stub_29 db "[MASM] CoT_Initialize_Core",0
sz_stub_30 db "[MASM] CoT_SelectCopyEngine",0
sz_stub_31 db "[MASM] CoT_Shutdown_Core",0
sz_stub_32 db "[MASM] CoT_TLS_SetError",0
sz_stub_33 db "[MASM] CoT_UpdateTelemetry",0
sz_stub_34 db "[MASM] DefinitionProvider_Adapter_Create",0
sz_stub_35 db "[MASM] DependencyGraph_AddNode",0
sz_stub_36 db "[MASM] DependencyGraph_Create",0
sz_stub_37 db "[MASM] DirectIO_Prefetch",0
sz_stub_38 db "[MASM] DiskExplorer_Init",0
sz_stub_39 db "[MASM] DiskExplorer_ScanDrives",0
sz_stub_40 db "[MASM] DiskKernel_AsyncReadSectors",0
sz_stub_41 db "[MASM] DiskKernel_DetectPartitions",0
sz_stub_42 db "[MASM] DiskKernel_EnumerateDrives",0
sz_stub_43 db "[MASM] DiskKernel_GetAsyncStatus",0
sz_stub_44 db "[MASM] DiskKernel_Init",0
sz_stub_45 db "[MASM] DiskKernel_Shutdown",0
sz_stub_46 db "[MASM] DiskRecovery_Abort",0
sz_stub_47 db "[MASM] DiskRecovery_Cleanup",0
sz_stub_48 db "[MASM] DiskRecovery_ExtractKey",0
sz_stub_49 db "[MASM] DiskRecovery_FindDrive",0
sz_stub_50 db "[MASM] DiskRecovery_GetStats",0
sz_stub_51 db "[MASM] DiskRecovery_Init",0
sz_stub_52 db "[MASM] DiskRecovery_Run",0
sz_stub_53 db "[MASM] DispatchComputeStage",0
sz_stub_54 db "[MASM] Disposable_Create",0
sz_stub_55 db "[MASM] DisposableCollection_Create",0
sz_stub_56 db "[MASM] DisposableCollection_Dispose",0
sz_stub_57 db "[MASM] EstimateRAM_Safe",0
sz_stub_58 db "[MASM] EventFire_ExtensionActivated",0
sz_stub_59 db "[MASM] EventFire_ExtensionDeactivated",0
sz_stub_60 db "[MASM] EventListener_DisposeInternal",0
sz_stub_61 db "[MASM] Extension_CleanupLanguageClients",0
sz_stub_62 db "[MASM] Extension_CleanupWebviews",0
sz_stub_63 db "[MASM] Extension_GetCurrent",0
sz_stub_64 db "[MASM] Extension_ValidateCapabilities",0
sz_stub_65 db "[MASM] ExtensionContext_Create",0
sz_stub_66 db "[MASM] ExtensionHostBridge_ProcessMessages",0
sz_stub_67 db "[MASM] ExtensionHostBridge_RegisterWebview",0
sz_stub_68 db "[MASM] ExtensionHostBridge_SendMessage",0
sz_stub_69 db "[MASM] ExtensionHostBridge_SendNotification",0
sz_stub_70 db "[MASM] ExtensionHostBridge_SendRequest",0
sz_stub_71 db "[MASM] ExtensionManifest_FromJson",0
sz_stub_72 db "[MASM] ExtensionModule_Load",0
sz_stub_73 db "[MASM] ExtensionStorage_GetPath",0
sz_stub_74 db "[MASM] find_pattern_asm",0
sz_stub_75 db "[MASM] fnv1a_hash64",0
sz_stub_76 db "[MASM] GenerateTokens",0
sz_stub_77 db "[MASM] GetBurstCount",0
sz_stub_78 db "[MASM] GetBurstPlan",0
sz_stub_79 db "[MASM] GetElapsedMicroseconds",0
sz_stub_80 db "[MASM] GetTensorOffset",0
sz_stub_81 db "[MASM] GetTensorSize",0
sz_stub_82 db "[MASM] GGUF_LoadFile",0
sz_stub_83 db "[MASM] HashMap_Create",0
sz_stub_84 db "[MASM] HashMap_ForEach",0
sz_stub_85 db "[MASM] HashMap_Get",0
sz_stub_86 db "[MASM] HashMap_Put",0
sz_stub_87 db "[MASM] HashMap_Remove",0
sz_stub_88 db "[MASM] HoverProvider_Adapter_Create",0
sz_stub_89 db "[MASM] HttpRouter_Initialize",0
sz_stub_90 db "[MASM] HybridCPU_MatMul",0
sz_stub_91 db "[MASM] HybridGPU_Init",0
sz_stub_92 db "[MASM] HybridGPU_MatMul",0
sz_stub_93 db "[MASM] HybridGPU_Synchronize",0
sz_stub_94 db "[MASM] Inference_Initialize",0
sz_stub_95 db "[MASM] InferenceEngine_Submit",0
sz_stub_96 db "[MASM] JoinCluster",0
sz_stub_97 db "[MASM] Json_GetArray",0
sz_stub_98 db "[MASM] Json_GetArrayField",0
sz_stub_99 db "[MASM] Json_GetInt",0
sz_stub_100 db "[MASM] Json_GetObjectField",0
sz_stub_101 db "[MASM] Json_GetObjectKeys",0
sz_stub_102 db "[MASM] Json_GetString",0
sz_stub_103 db "[MASM] Json_GetStringField",0
sz_stub_104 db "[MASM] Json_HasField",0
sz_stub_105 db "[MASM] Json_ParseFile",0
sz_stub_106 db "[MASM] Json_ParseObject",0
sz_stub_107 db "[MASM] Json_ParseString",0
sz_stub_108 db "[MASM] JsonObject_Create",0
sz_stub_109 db "[MASM] LoadTensorBlock",0
sz_stub_110 db "[MASM] LSP_Handshake_Sequence",0
sz_stub_111 db "[MASM] LSP_JsonRpc_BuildNotification",0
sz_stub_112 db "[MASM] LSP_Transport_Write",0
sz_stub_113 db "[MASM] LspClient_ForwardMessage",0
sz_stub_114 db "[MASM] Marketplace_DownloadExtension",0
sz_stub_115 db "[MASM] Math_InitTables",0
sz_stub_116 db "[MASM] ModelBridge_GetProfile",0
sz_stub_117 db "[MASM] ModelBridge_Init",0
sz_stub_118 db "[MASM] ModelBridge_LoadModel",0
sz_stub_119 db "[MASM] ModelBridge_UnloadModel",0
sz_stub_120 db "[MASM] ModelBridge_ValidateLoad",0
sz_stub_121 db "[MASM] ModelState_AcquireInstance",0
sz_stub_122 db "[MASM] ModelState_Initialize",0
sz_stub_123 db "[MASM] ModelState_Transition",0
sz_stub_124 db "[MASM] NanoDisk_AbortJob",0
sz_stub_125 db "[MASM] NanoDisk_GetJobResult",0
sz_stub_126 db "[MASM] NanoDisk_GetJobStatus",0
sz_stub_127 db "[MASM] NanoDisk_Init",0
sz_stub_128 db "[MASM] NanoDisk_Shutdown",0
sz_stub_129 db "[MASM] NanoQuant_DequantizeMatMul",0
sz_stub_130 db "[MASM] NanoQuant_DequantizeTensor",0
sz_stub_131 db "[MASM] NanoQuant_GetCompressionRatio",0
sz_stub_132 db "[MASM] NanoQuant_QuantizeTensor",0
sz_stub_133 db "[MASM] NVMe_GetTemperature",0
sz_stub_134 db "[MASM] NVMe_GetWearLevel",0
sz_stub_135 db "[MASM] Observable_Create_ActiveTextEditor",0
sz_stub_136 db "[MASM] Observable_Create_VisibleTextEditors",0
sz_stub_137 db "[MASM] Observable_Create_WorkspaceFolders",0
sz_stub_138 db "[MASM] OrchestratorInitialize",0
sz_stub_139 db "[MASM] OutputChannel_Append",0
sz_stub_140 db "[MASM] OutputChannel_AppendLine",0
sz_stub_141 db "[MASM] OutputChannel_Create",0
sz_stub_142 db "[MASM] OutputChannel_CreateAPI",0
sz_stub_143 db "[MASM] Path_Join",0
sz_stub_144 db "[MASM] Path_Join_PackageJson",0
sz_stub_145 db "[MASM] Phase1Initialize",0
sz_stub_146 db "[MASM] Phase1LogMessage",0
sz_stub_147 db "[MASM] Phase2Initialize",0
sz_stub_148 db "[MASM] Phase3Initialize",0
sz_stub_149 db "[MASM] Phase4Initialize",0
sz_stub_150 db "[MASM] Pipe_RunServer",0
sz_stub_151 db "[MASM] PrintU64",0
sz_stub_152 db "[MASM] ProcessReceivedHeartbeat",0
sz_stub_153 db "[MASM] ProcessSwarmQueue",0
sz_stub_154 db "[MASM] Provider_FromDocumentSelector",0
sz_stub_155 db "[MASM] Provider_Register",0
sz_stub_156 db "[MASM] QueueInferenceJob",0
sz_stub_157 db "[MASM] RaftEventLoop",0
sz_stub_158 db "[MASM] RawrXD_Calc_ContentLength",0
sz_stub_159 db "[MASM] rawrxd_dispatch_cli",0
sz_stub_160 db "[MASM] rawrxd_dispatch_command",0
sz_stub_161 db "[MASM] rawrxd_dispatch_feature",0
sz_stub_162 db "[MASM] rawrxd_get_feature_count",0
sz_stub_163 db "[MASM] RawrXD_JSON_Stringify",0
sz_stub_164 db "[MASM] RawrXD_Marketplace_ResolveSymbol",0
sz_stub_165 db "[MASM] RawrXD_UI_Push_Notify",0
sz_stub_166 db "[MASM] ReadTsc",0
sz_stub_167 db "[MASM] Registry_CreateKey",0
sz_stub_168 db "[MASM] Registry_KeyExists",0
sz_stub_169 db "[MASM] Registry_SetDwordValue",0
sz_stub_170 db "[MASM] Registry_SetQwordValue",0
sz_stub_171 db "[MASM] Registry_SetStringValue",0
sz_stub_172 db "[MASM] Release_CoT_Lock",0
sz_stub_173 db "[MASM] Release_CoT_Lock_Shared",0
sz_stub_174 db "[MASM] ResolveZonePointer",0
sz_stub_175 db "[MASM] RingBufferConsumer_Initialize",0
sz_stub_176 db "[MASM] RingBufferConsumer_Shutdown",0
sz_stub_177 db "[MASM] RouteModelLoad",0
sz_stub_178 db "[MASM] Sample_Logits_TopP",0
sz_stub_179 db "[MASM] SemVer_Parse",0
sz_stub_180 db "[MASM] SemVer_ParseRange",0
sz_stub_181 db "[MASM] SemVer_Satisfies",0
sz_stub_182 db "[MASM] ShellInteg_CompleteCommand",0
sz_stub_183 db "[MASM] ShellInteg_ExecuteCommand",0
sz_stub_184 db "[MASM] ShellInteg_GetCommandHistory",0
sz_stub_185 db "[MASM] ShellInteg_GetStats",0
sz_stub_186 db "[MASM] ShellInteg_IsAlive",0
sz_stub_187 db "[MASM] Shield_AES_DecryptShim",0
sz_stub_188 db "[MASM] Shield_GenerateHWID",0
sz_stub_189 db "[MASM] Shield_TimingCheck",0
sz_stub_190 db "[MASM] Shield_VerifyIntegrity",0
sz_stub_191 db "[MASM] SidecarMain",0
sz_stub_192 db "[MASM] Spinlock_Acquire",0
sz_stub_193 db "[MASM] Spinlock_Release",0
sz_stub_194 db "[MASM] StartPipeServer",0
sz_stub_195 db "[MASM] StreamFormatter_WriteToken",0
sz_stub_196 db "[MASM] StreamTensorByName",0
sz_stub_197 db "[MASM] SubmitInferenceRequest",0
sz_stub_198 db "[MASM] SubmitTask",0
sz_stub_199 db "[MASM] Swarm_Initialize",0
sz_stub_200 db "[MASM] Swarm_SubmitJob",0
sz_stub_201 db "[MASM] SwarmTransportControl",0
sz_stub_202 db "[MASM] System_InitializePrimitives",0
sz_stub_203 db "[MASM] Telemetry_SanitizeData",0
sz_stub_204 db "[MASM] Titan_DirectStorage_Cleanup",0
sz_stub_205 db "[MASM] Titan_GGML_Cleanup",0
sz_stub_206 db "[MASM] Titan_InferenceThread",0
sz_stub_207 db "[MASM] Titan_Initialize",0
sz_stub_208 db "[MASM] Titan_LoadModel",0
sz_stub_209 db "[MASM] Titan_RunInference",0
sz_stub_210 db "[MASM] Titan_RunInferenceStep",0
sz_stub_211 db "[MASM] Titan_Shutdown",0
sz_stub_212 db "[MASM] Titan_Stop_All_Streams",0
sz_stub_213 db "[MASM] Titan_SubmitPrompt",0
sz_stub_214 db "[MASM] Titan_Vulkan_Cleanup",0
sz_stub_215 db "[MASM] Unlock_800B_Kernel",0
sz_stub_216 db "[MASM] ValidateModelAlignment",0
sz_stub_217 db "[MASM] Vram_Allocate",0
sz_stub_218 db "[MASM] Vram_Initialize",0
sz_stub_219 db "[MASM] VulkanDMA_RegisterTensor",0
sz_stub_220 db "[MASM] VulkanKernel_AllocBuffer",0
sz_stub_221 db "[MASM] VulkanKernel_Cleanup",0
sz_stub_222 db "[MASM] VulkanKernel_CopyToDevice",0
sz_stub_223 db "[MASM] VulkanKernel_CopyToHost",0
sz_stub_224 db "[MASM] VulkanKernel_CreatePipeline",0
sz_stub_225 db "[MASM] VulkanKernel_DispatchFlashAttn",0
sz_stub_226 db "[MASM] VulkanKernel_DispatchMatMul",0
sz_stub_227 db "[MASM] VulkanKernel_GetStats",0
sz_stub_228 db "[MASM] VulkanKernel_HotswapShader",0
sz_stub_229 db "[MASM] VulkanKernel_Init",0
sz_stub_230 db "[MASM] VulkanKernel_LoadShader",0
sz_stub_231 db "[MASM] WebviewPanel_CreateAPI",0
sz_stub_232 db "[MASM] Week1Initialize",0
sz_stub_233 db "[MASM] Week23Initialize",0
sz_stub_arena_allocate db "[MASM] ArenaAllocate",0

.code
PUBLIC RawrXD_AccelRouter_Create_MASM
PUBLIC RawrXD_AccelRouter_ForceBackend_MASM
PUBLIC RawrXD_AccelRouter_GetActiveBackend_MASM
PUBLIC RawrXD_AccelRouter_GetStatsJson_MASM
PUBLIC RawrXD_AccelRouter_Init_MASM
PUBLIC RawrXD_AccelRouter_IsBackendAvailable_MASM
PUBLIC RawrXD_AccelRouter_Shutdown_MASM
PUBLIC RawrXD_AccelRouter_Submit_MASM
PUBLIC RawrXD_Acquire_CoT_Lock_MASM
PUBLIC RawrXD_Acquire_CoT_Lock_Shared_MASM
PUBLIC RawrXD_AgentRouter_ExecuteTask_MASM
PUBLIC RawrXD_AgentRouter_Initialize_MASM
PUBLIC RawrXD_AgentTool_QuantizeModel_MASM
PUBLIC RawrXD_Apply_FFN_SwiGLU_MASM
PUBLIC RawrXD_Apply_RMSNorm_MASM
PUBLIC RawrXD_Apply_RoPE_Direct_MASM
PUBLIC RawrXD_ArrayList_Add_MASM
PUBLIC RawrXD_ArrayList_Clear_MASM
PUBLIC RawrXD_ArrayList_Create_MASM
PUBLIC RawrXD_asm_apply_memory_patch_MASM
PUBLIC RawrXD_asm_camellia256_decrypt_ctr_MASM
PUBLIC RawrXD_asm_camellia256_encrypt_ctr_MASM
PUBLIC RawrXD_asm_camellia256_get_hmac_key_MASM
PUBLIC RawrXD_CleanupInference_MASM
PUBLIC RawrXD_CompletionProvider_Adapter_Create_MASM
PUBLIC RawrXD_Compute_MHA_Parallel_MASM
PUBLIC RawrXD_ConsolePrint_MASM
PUBLIC RawrXD_CoT_EnableMultiProducer_MASM
PUBLIC RawrXD_CoT_Has_Large_Pages_MASM
PUBLIC RawrXD_CoT_Initialize_Core_MASM
PUBLIC RawrXD_CoT_SelectCopyEngine_MASM
PUBLIC RawrXD_CoT_Shutdown_Core_MASM
PUBLIC RawrXD_CoT_TLS_SetError_MASM
PUBLIC RawrXD_CoT_UpdateTelemetry_MASM
PUBLIC RawrXD_DefinitionProvider_Adapter_Create_MASM
PUBLIC RawrXD_DependencyGraph_AddNode_MASM
PUBLIC RawrXD_DependencyGraph_Create_MASM
PUBLIC RawrXD_DirectIO_Prefetch_MASM
PUBLIC RawrXD_DiskExplorer_Init_MASM
PUBLIC RawrXD_DiskExplorer_ScanDrives_MASM
PUBLIC RawrXD_DiskKernel_AsyncReadSectors_MASM
PUBLIC RawrXD_DiskKernel_DetectPartitions_MASM
PUBLIC RawrXD_DiskKernel_EnumerateDrives_MASM
PUBLIC RawrXD_DiskKernel_GetAsyncStatus_MASM
PUBLIC RawrXD_DiskKernel_Init_MASM
PUBLIC RawrXD_DiskKernel_Shutdown_MASM
PUBLIC RawrXD_DiskRecovery_Abort_MASM
PUBLIC RawrXD_DiskRecovery_Cleanup_MASM
PUBLIC RawrXD_DiskRecovery_ExtractKey_MASM
PUBLIC RawrXD_DiskRecovery_FindDrive_MASM
PUBLIC RawrXD_DiskRecovery_GetStats_MASM
PUBLIC RawrXD_DiskRecovery_Init_MASM
PUBLIC RawrXD_DiskRecovery_Run_MASM
PUBLIC RawrXD_DispatchComputeStage_MASM
PUBLIC RawrXD_Disposable_Create_MASM
PUBLIC RawrXD_DisposableCollection_Create_MASM
PUBLIC RawrXD_DisposableCollection_Dispose_MASM
PUBLIC RawrXD_EstimateRAM_Safe_MASM
PUBLIC RawrXD_EventFire_ExtensionActivated_MASM
PUBLIC RawrXD_EventFire_ExtensionDeactivated_MASM
PUBLIC RawrXD_EventListener_DisposeInternal_MASM
PUBLIC RawrXD_Extension_CleanupLanguageClients_MASM
PUBLIC RawrXD_Extension_CleanupWebviews_MASM
PUBLIC RawrXD_Extension_GetCurrent_MASM
PUBLIC RawrXD_Extension_ValidateCapabilities_MASM
PUBLIC RawrXD_ExtensionContext_Create_MASM
PUBLIC RawrXD_ExtensionHostBridge_ProcessMessages_MASM
PUBLIC RawrXD_ExtensionHostBridge_RegisterWebview_MASM
PUBLIC RawrXD_ExtensionHostBridge_SendMessage_MASM
PUBLIC RawrXD_ExtensionHostBridge_SendNotification_MASM
PUBLIC RawrXD_ExtensionHostBridge_SendRequest_MASM
PUBLIC RawrXD_ExtensionManifest_FromJson_MASM
PUBLIC RawrXD_ExtensionModule_Load_MASM
PUBLIC RawrXD_ExtensionStorage_GetPath_MASM
PUBLIC RawrXD_find_pattern_asm_MASM
PUBLIC RawrXD_fnv1a_hash64_MASM
PUBLIC RawrXD_GenerateTokens_MASM
PUBLIC RawrXD_GetBurstCount_MASM
PUBLIC RawrXD_GetBurstPlan_MASM
PUBLIC RawrXD_GetElapsedMicroseconds_MASM
PUBLIC RawrXD_GetTensorOffset_MASM
PUBLIC RawrXD_GetTensorSize_MASM
PUBLIC RawrXD_GGUF_LoadFile_MASM
PUBLIC RawrXD_HashMap_Create_MASM
PUBLIC RawrXD_HashMap_ForEach_MASM
PUBLIC RawrXD_HashMap_Get_MASM
PUBLIC RawrXD_HashMap_Put_MASM
PUBLIC RawrXD_HashMap_Remove_MASM
PUBLIC RawrXD_HoverProvider_Adapter_Create_MASM
PUBLIC RawrXD_HttpRouter_Initialize_MASM
PUBLIC RawrXD_HybridCPU_MatMul_MASM
PUBLIC RawrXD_HybridGPU_Init_MASM
PUBLIC RawrXD_HybridGPU_MatMul_MASM
PUBLIC RawrXD_HybridGPU_Synchronize_MASM
PUBLIC RawrXD_Inference_Initialize_MASM
PUBLIC RawrXD_InferenceEngine_Submit_MASM
PUBLIC RawrXD_JoinCluster_MASM
PUBLIC RawrXD_Json_GetArray_MASM
PUBLIC RawrXD_Json_GetArrayField_MASM
PUBLIC RawrXD_Json_GetInt_MASM
PUBLIC RawrXD_Json_GetObjectField_MASM
PUBLIC RawrXD_Json_GetObjectKeys_MASM
PUBLIC RawrXD_Json_GetString_MASM
PUBLIC RawrXD_Json_GetStringField_MASM
PUBLIC RawrXD_Json_HasField_MASM
PUBLIC RawrXD_Json_ParseFile_MASM
PUBLIC RawrXD_Json_ParseObject_MASM
PUBLIC RawrXD_Json_ParseString_MASM
PUBLIC RawrXD_JsonObject_Create_MASM
PUBLIC RawrXD_LoadTensorBlock_MASM
PUBLIC RawrXD_LSP_Handshake_Sequence_MASM
PUBLIC RawrXD_LSP_JsonRpc_BuildNotification_MASM
PUBLIC RawrXD_LSP_Transport_Write_MASM
PUBLIC RawrXD_LspClient_ForwardMessage_MASM
PUBLIC RawrXD_Marketplace_DownloadExtension_MASM
PUBLIC RawrXD_Math_InitTables_MASM
PUBLIC RawrXD_ModelBridge_GetProfile_MASM
PUBLIC RawrXD_ModelBridge_Init_MASM
PUBLIC RawrXD_ModelBridge_LoadModel_MASM
PUBLIC RawrXD_ModelBridge_UnloadModel_MASM
PUBLIC RawrXD_ModelBridge_ValidateLoad_MASM
PUBLIC RawrXD_ModelState_AcquireInstance_MASM
PUBLIC RawrXD_ModelState_Initialize_MASM
PUBLIC RawrXD_ModelState_Transition_MASM
PUBLIC RawrXD_NanoDisk_AbortJob_MASM
PUBLIC RawrXD_NanoDisk_GetJobResult_MASM
PUBLIC RawrXD_NanoDisk_GetJobStatus_MASM
PUBLIC RawrXD_NanoDisk_Init_MASM
PUBLIC RawrXD_NanoDisk_Shutdown_MASM
PUBLIC RawrXD_NanoQuant_DequantizeMatMul_MASM
PUBLIC RawrXD_NanoQuant_DequantizeTensor_MASM
PUBLIC RawrXD_NanoQuant_GetCompressionRatio_MASM
PUBLIC RawrXD_NanoQuant_QuantizeTensor_MASM
PUBLIC RawrXD_NVMe_GetTemperature_MASM
PUBLIC RawrXD_NVMe_GetWearLevel_MASM
PUBLIC RawrXD_Observable_Create_ActiveTextEditor_MASM
PUBLIC RawrXD_Observable_Create_VisibleTextEditors_MASM
PUBLIC RawrXD_Observable_Create_WorkspaceFolders_MASM
PUBLIC RawrXD_OrchestratorInitialize_MASM
PUBLIC RawrXD_OutputChannel_Append_MASM
PUBLIC RawrXD_OutputChannel_AppendLine_MASM
PUBLIC RawrXD_OutputChannel_Create_MASM
PUBLIC RawrXD_OutputChannel_CreateAPI_MASM
PUBLIC RawrXD_Path_Join_MASM
PUBLIC RawrXD_Path_Join_PackageJson_MASM
PUBLIC RawrXD_Phase1Initialize_MASM
PUBLIC RawrXD_Phase1LogMessage_MASM
PUBLIC RawrXD_Phase2Initialize_MASM
PUBLIC RawrXD_Phase3Initialize_MASM
PUBLIC RawrXD_Phase4Initialize_MASM
PUBLIC RawrXD_Pipe_RunServer_MASM
PUBLIC RawrXD_PrintU64_MASM
PUBLIC RawrXD_ProcessReceivedHeartbeat_MASM
PUBLIC RawrXD_ProcessSwarmQueue_MASM
PUBLIC RawrXD_Provider_FromDocumentSelector_MASM
PUBLIC RawrXD_Provider_Register_MASM
PUBLIC RawrXD_QueueInferenceJob_MASM
PUBLIC RawrXD_RaftEventLoop_MASM
PUBLIC RawrXD_RawrXD_Calc_ContentLength_MASM
PUBLIC RawrXD_rawrxd_dispatch_cli_MASM
PUBLIC RawrXD_rawrxd_dispatch_command_MASM
PUBLIC RawrXD_rawrxd_dispatch_feature_MASM
PUBLIC RawrXD_rawrxd_get_feature_count_MASM
PUBLIC RawrXD_RawrXD_JSON_Stringify_MASM
PUBLIC RawrXD_RawrXD_Marketplace_ResolveSymbol_MASM
PUBLIC RawrXD_RawrXD_UI_Push_Notify_MASM
PUBLIC RawrXD_ReadTsc_MASM
PUBLIC RawrXD_Registry_CreateKey_MASM
PUBLIC RawrXD_Registry_KeyExists_MASM
PUBLIC RawrXD_Registry_SetDwordValue_MASM
PUBLIC RawrXD_Registry_SetQwordValue_MASM
PUBLIC RawrXD_Registry_SetStringValue_MASM
PUBLIC RawrXD_Release_CoT_Lock_MASM
PUBLIC RawrXD_Release_CoT_Lock_Shared_MASM
PUBLIC RawrXD_ResolveZonePointer_MASM
PUBLIC RawrXD_RingBufferConsumer_Initialize_MASM
PUBLIC RawrXD_RingBufferConsumer_Shutdown_MASM
PUBLIC RawrXD_RouteModelLoad_MASM
PUBLIC RawrXD_Sample_Logits_TopP_MASM
PUBLIC RawrXD_SemVer_Parse_MASM
PUBLIC RawrXD_SemVer_ParseRange_MASM
PUBLIC RawrXD_SemVer_Satisfies_MASM
PUBLIC RawrXD_ShellInteg_CompleteCommand_MASM
PUBLIC RawrXD_ShellInteg_ExecuteCommand_MASM
PUBLIC RawrXD_ShellInteg_GetCommandHistory_MASM
PUBLIC RawrXD_ShellInteg_GetStats_MASM
PUBLIC RawrXD_ShellInteg_IsAlive_MASM
PUBLIC RawrXD_Shield_AES_DecryptShim_MASM
PUBLIC RawrXD_Shield_GenerateHWID_MASM
PUBLIC RawrXD_Shield_TimingCheck_MASM
PUBLIC RawrXD_Shield_VerifyIntegrity_MASM
PUBLIC RawrXD_SidecarMain_MASM
PUBLIC RawrXD_Spinlock_Acquire_MASM
PUBLIC RawrXD_Spinlock_Release_MASM
PUBLIC RawrXD_StartPipeServer_MASM
PUBLIC RawrXD_StreamFormatter_WriteToken_MASM
PUBLIC RawrXD_StreamTensorByName_MASM
PUBLIC RawrXD_SubmitInferenceRequest_MASM
PUBLIC RawrXD_SubmitTask_MASM
PUBLIC RawrXD_Swarm_Initialize_MASM
PUBLIC RawrXD_Swarm_SubmitJob_MASM
PUBLIC RawrXD_SwarmTransportControl_MASM
PUBLIC RawrXD_System_InitializePrimitives_MASM
PUBLIC RawrXD_Telemetry_SanitizeData_MASM
PUBLIC RawrXD_Titan_DirectStorage_Cleanup_MASM
PUBLIC RawrXD_Titan_GGML_Cleanup_MASM
PUBLIC RawrXD_Titan_InferenceThread_MASM
PUBLIC RawrXD_Titan_Initialize_MASM
PUBLIC RawrXD_Titan_LoadModel_MASM
PUBLIC RawrXD_Titan_RunInference_MASM
PUBLIC RawrXD_Titan_RunInferenceStep_MASM
PUBLIC RawrXD_Titan_Shutdown_MASM
PUBLIC RawrXD_Titan_Stop_All_Streams_MASM
PUBLIC RawrXD_Titan_SubmitPrompt_MASM
PUBLIC RawrXD_Titan_Vulkan_Cleanup_MASM
PUBLIC RawrXD_Unlock_800B_Kernel_MASM
PUBLIC RawrXD_ValidateModelAlignment_MASM
PUBLIC RawrXD_Vram_Allocate_MASM
PUBLIC RawrXD_Vram_Initialize_MASM
PUBLIC RawrXD_VulkanDMA_RegisterTensor_MASM
PUBLIC RawrXD_VulkanKernel_AllocBuffer_MASM
PUBLIC RawrXD_VulkanKernel_Cleanup_MASM
PUBLIC RawrXD_VulkanKernel_CopyToDevice_MASM
PUBLIC RawrXD_VulkanKernel_CopyToHost_MASM
PUBLIC RawrXD_VulkanKernel_CreatePipeline_MASM
PUBLIC RawrXD_VulkanKernel_DispatchFlashAttn_MASM
PUBLIC RawrXD_VulkanKernel_DispatchMatMul_MASM
PUBLIC RawrXD_VulkanKernel_GetStats_MASM
PUBLIC RawrXD_VulkanKernel_HotswapShader_MASM
PUBLIC RawrXD_VulkanKernel_Init_MASM
PUBLIC RawrXD_VulkanKernel_LoadShader_MASM
PUBLIC RawrXD_WebviewPanel_CreateAPI_MASM
PUBLIC RawrXD_Week1Initialize_MASM
PUBLIC RawrXD_Week23Initialize_MASM
PUBLIC RawrXD_ArenaAllocate_MASM

RawrXD_AccelRouter_Create_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_0
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_AccelRouter_Create_MASM ENDP

RawrXD_AccelRouter_ForceBackend_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_1
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_AccelRouter_ForceBackend_MASM ENDP

RawrXD_AccelRouter_GetActiveBackend_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_2
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_AccelRouter_GetActiveBackend_MASM ENDP

RawrXD_AccelRouter_GetStatsJson_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_3
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_AccelRouter_GetStatsJson_MASM ENDP

RawrXD_AccelRouter_Init_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_4
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_AccelRouter_Init_MASM ENDP

RawrXD_AccelRouter_IsBackendAvailable_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_5
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_AccelRouter_IsBackendAvailable_MASM ENDP

RawrXD_AccelRouter_Shutdown_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_6
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_AccelRouter_Shutdown_MASM ENDP

RawrXD_AccelRouter_Submit_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_7
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_AccelRouter_Submit_MASM ENDP

RawrXD_Acquire_CoT_Lock_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_8
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Acquire_CoT_Lock_MASM ENDP

RawrXD_Acquire_CoT_Lock_Shared_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_9
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Acquire_CoT_Lock_Shared_MASM ENDP

RawrXD_AgentRouter_ExecuteTask_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_10
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_AgentRouter_ExecuteTask_MASM ENDP

RawrXD_AgentRouter_Initialize_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_11
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_AgentRouter_Initialize_MASM ENDP

RawrXD_AgentTool_QuantizeModel_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_12
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_AgentTool_QuantizeModel_MASM ENDP

RawrXD_Apply_FFN_SwiGLU_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_13
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Apply_FFN_SwiGLU_MASM ENDP

RawrXD_Apply_RMSNorm_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_14
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Apply_RMSNorm_MASM ENDP

RawrXD_Apply_RoPE_Direct_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_15
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Apply_RoPE_Direct_MASM ENDP

RawrXD_ArrayList_Add_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_16
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ArrayList_Add_MASM ENDP

RawrXD_ArrayList_Clear_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_17
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ArrayList_Clear_MASM ENDP

RawrXD_ArrayList_Create_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_18
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ArrayList_Create_MASM ENDP

RawrXD_asm_apply_memory_patch_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_19
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_asm_apply_memory_patch_MASM ENDP

RawrXD_asm_camellia256_decrypt_ctr_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_20
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_asm_camellia256_decrypt_ctr_MASM ENDP

RawrXD_asm_camellia256_encrypt_ctr_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_21
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_asm_camellia256_encrypt_ctr_MASM ENDP

RawrXD_asm_camellia256_get_hmac_key_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_22
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_asm_camellia256_get_hmac_key_MASM ENDP

RawrXD_CleanupInference_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_23
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_CleanupInference_MASM ENDP

RawrXD_CompletionProvider_Adapter_Create_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_24
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_CompletionProvider_Adapter_Create_MASM ENDP

RawrXD_Compute_MHA_Parallel_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_25
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Compute_MHA_Parallel_MASM ENDP

RawrXD_ConsolePrint_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_26
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ConsolePrint_MASM ENDP

RawrXD_CoT_EnableMultiProducer_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_27
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_CoT_EnableMultiProducer_MASM ENDP

RawrXD_CoT_Has_Large_Pages_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_28
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_CoT_Has_Large_Pages_MASM ENDP

RawrXD_CoT_Initialize_Core_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_29
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_CoT_Initialize_Core_MASM ENDP

RawrXD_CoT_SelectCopyEngine_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_30
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_CoT_SelectCopyEngine_MASM ENDP

RawrXD_CoT_Shutdown_Core_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_31
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_CoT_Shutdown_Core_MASM ENDP

RawrXD_CoT_TLS_SetError_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_32
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_CoT_TLS_SetError_MASM ENDP

RawrXD_CoT_UpdateTelemetry_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_33
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_CoT_UpdateTelemetry_MASM ENDP

RawrXD_DefinitionProvider_Adapter_Create_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_34
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_DefinitionProvider_Adapter_Create_MASM ENDP

RawrXD_DependencyGraph_AddNode_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_35
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_DependencyGraph_AddNode_MASM ENDP

RawrXD_DependencyGraph_Create_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_36
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_DependencyGraph_Create_MASM ENDP

RawrXD_DirectIO_Prefetch_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_37
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_DirectIO_Prefetch_MASM ENDP

RawrXD_DiskExplorer_Init_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_38
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_DiskExplorer_Init_MASM ENDP

RawrXD_DiskExplorer_ScanDrives_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_39
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_DiskExplorer_ScanDrives_MASM ENDP

RawrXD_DiskKernel_AsyncReadSectors_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_40
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_DiskKernel_AsyncReadSectors_MASM ENDP

RawrXD_DiskKernel_DetectPartitions_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_41
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_DiskKernel_DetectPartitions_MASM ENDP

RawrXD_DiskKernel_EnumerateDrives_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_42
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_DiskKernel_EnumerateDrives_MASM ENDP

RawrXD_DiskKernel_GetAsyncStatus_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_43
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_DiskKernel_GetAsyncStatus_MASM ENDP

RawrXD_DiskKernel_Init_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_44
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_DiskKernel_Init_MASM ENDP

RawrXD_DiskKernel_Shutdown_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_45
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_DiskKernel_Shutdown_MASM ENDP

RawrXD_DiskRecovery_Abort_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_46
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_DiskRecovery_Abort_MASM ENDP

RawrXD_DiskRecovery_Cleanup_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_47
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_DiskRecovery_Cleanup_MASM ENDP

RawrXD_DiskRecovery_ExtractKey_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_48
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_DiskRecovery_ExtractKey_MASM ENDP

RawrXD_DiskRecovery_FindDrive_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_49
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_DiskRecovery_FindDrive_MASM ENDP

RawrXD_DiskRecovery_GetStats_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_50
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_DiskRecovery_GetStats_MASM ENDP

RawrXD_DiskRecovery_Init_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_51
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_DiskRecovery_Init_MASM ENDP

RawrXD_DiskRecovery_Run_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_52
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_DiskRecovery_Run_MASM ENDP

RawrXD_DispatchComputeStage_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_53
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_DispatchComputeStage_MASM ENDP

RawrXD_Disposable_Create_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_54
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Disposable_Create_MASM ENDP

RawrXD_DisposableCollection_Create_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_55
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_DisposableCollection_Create_MASM ENDP

RawrXD_DisposableCollection_Dispose_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_56
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_DisposableCollection_Dispose_MASM ENDP

RawrXD_EstimateRAM_Safe_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_57
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_EstimateRAM_Safe_MASM ENDP

RawrXD_EventFire_ExtensionActivated_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_58
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_EventFire_ExtensionActivated_MASM ENDP

RawrXD_EventFire_ExtensionDeactivated_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_59
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_EventFire_ExtensionDeactivated_MASM ENDP

RawrXD_EventListener_DisposeInternal_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_60
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_EventListener_DisposeInternal_MASM ENDP

RawrXD_Extension_CleanupLanguageClients_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_61
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Extension_CleanupLanguageClients_MASM ENDP

RawrXD_Extension_CleanupWebviews_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_62
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Extension_CleanupWebviews_MASM ENDP

RawrXD_Extension_GetCurrent_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_63
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Extension_GetCurrent_MASM ENDP

RawrXD_Extension_ValidateCapabilities_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_64
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Extension_ValidateCapabilities_MASM ENDP

RawrXD_ExtensionContext_Create_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_65
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ExtensionContext_Create_MASM ENDP

RawrXD_ExtensionHostBridge_ProcessMessages_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_66
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ExtensionHostBridge_ProcessMessages_MASM ENDP

RawrXD_ExtensionHostBridge_RegisterWebview_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_67
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ExtensionHostBridge_RegisterWebview_MASM ENDP

RawrXD_ExtensionHostBridge_SendMessage_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_68
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ExtensionHostBridge_SendMessage_MASM ENDP

RawrXD_ExtensionHostBridge_SendNotification_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_69
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ExtensionHostBridge_SendNotification_MASM ENDP

RawrXD_ExtensionHostBridge_SendRequest_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_70
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ExtensionHostBridge_SendRequest_MASM ENDP

RawrXD_ExtensionManifest_FromJson_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_71
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ExtensionManifest_FromJson_MASM ENDP

RawrXD_ExtensionModule_Load_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_72
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ExtensionModule_Load_MASM ENDP

RawrXD_ExtensionStorage_GetPath_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_73
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ExtensionStorage_GetPath_MASM ENDP

RawrXD_find_pattern_asm_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_74
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_find_pattern_asm_MASM ENDP

RawrXD_fnv1a_hash64_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_75
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_fnv1a_hash64_MASM ENDP

RawrXD_GenerateTokens_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_76
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_GenerateTokens_MASM ENDP

RawrXD_GetBurstCount_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_77
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_GetBurstCount_MASM ENDP

RawrXD_GetBurstPlan_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_78
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_GetBurstPlan_MASM ENDP

RawrXD_GetElapsedMicroseconds_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_79
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_GetElapsedMicroseconds_MASM ENDP

RawrXD_GetTensorOffset_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_80
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_GetTensorOffset_MASM ENDP

RawrXD_GetTensorSize_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_81
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_GetTensorSize_MASM ENDP

RawrXD_GGUF_LoadFile_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_82
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_GGUF_LoadFile_MASM ENDP

RawrXD_HashMap_Create_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_83
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_HashMap_Create_MASM ENDP

RawrXD_HashMap_ForEach_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_84
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_HashMap_ForEach_MASM ENDP

RawrXD_HashMap_Get_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_85
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_HashMap_Get_MASM ENDP

RawrXD_HashMap_Put_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_86
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_HashMap_Put_MASM ENDP

RawrXD_HashMap_Remove_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_87
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_HashMap_Remove_MASM ENDP

RawrXD_HoverProvider_Adapter_Create_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_88
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_HoverProvider_Adapter_Create_MASM ENDP

RawrXD_HttpRouter_Initialize_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_89
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_HttpRouter_Initialize_MASM ENDP

RawrXD_HybridCPU_MatMul_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_90
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_HybridCPU_MatMul_MASM ENDP

RawrXD_HybridGPU_Init_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_91
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_HybridGPU_Init_MASM ENDP

RawrXD_HybridGPU_MatMul_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_92
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_HybridGPU_MatMul_MASM ENDP

RawrXD_HybridGPU_Synchronize_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_93
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_HybridGPU_Synchronize_MASM ENDP

RawrXD_Inference_Initialize_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_94
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Inference_Initialize_MASM ENDP

RawrXD_InferenceEngine_Submit_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_95
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_InferenceEngine_Submit_MASM ENDP

RawrXD_JoinCluster_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_96
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_JoinCluster_MASM ENDP

RawrXD_Json_GetArray_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_97
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Json_GetArray_MASM ENDP

RawrXD_Json_GetArrayField_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_98
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Json_GetArrayField_MASM ENDP

RawrXD_Json_GetInt_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_99
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Json_GetInt_MASM ENDP

RawrXD_Json_GetObjectField_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_100
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Json_GetObjectField_MASM ENDP

RawrXD_Json_GetObjectKeys_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_101
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Json_GetObjectKeys_MASM ENDP

RawrXD_Json_GetString_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_102
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Json_GetString_MASM ENDP

RawrXD_Json_GetStringField_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_103
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Json_GetStringField_MASM ENDP

RawrXD_Json_HasField_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_104
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Json_HasField_MASM ENDP

RawrXD_Json_ParseFile_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_105
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Json_ParseFile_MASM ENDP

RawrXD_Json_ParseObject_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_106
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Json_ParseObject_MASM ENDP

RawrXD_Json_ParseString_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_107
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Json_ParseString_MASM ENDP

RawrXD_JsonObject_Create_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_108
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_JsonObject_Create_MASM ENDP

RawrXD_LoadTensorBlock_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_109
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_LoadTensorBlock_MASM ENDP

RawrXD_LSP_Handshake_Sequence_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_110
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_LSP_Handshake_Sequence_MASM ENDP

RawrXD_LSP_JsonRpc_BuildNotification_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_111
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_LSP_JsonRpc_BuildNotification_MASM ENDP

RawrXD_LSP_Transport_Write_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_112
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_LSP_Transport_Write_MASM ENDP

RawrXD_LspClient_ForwardMessage_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_113
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_LspClient_ForwardMessage_MASM ENDP

RawrXD_Marketplace_DownloadExtension_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_114
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Marketplace_DownloadExtension_MASM ENDP

RawrXD_Math_InitTables_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_115
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Math_InitTables_MASM ENDP

RawrXD_ModelBridge_GetProfile_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_116
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ModelBridge_GetProfile_MASM ENDP

RawrXD_ModelBridge_Init_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_117
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ModelBridge_Init_MASM ENDP

RawrXD_ModelBridge_LoadModel_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_118
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ModelBridge_LoadModel_MASM ENDP

RawrXD_ModelBridge_UnloadModel_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_119
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ModelBridge_UnloadModel_MASM ENDP

RawrXD_ModelBridge_ValidateLoad_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_120
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ModelBridge_ValidateLoad_MASM ENDP

RawrXD_ModelState_AcquireInstance_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_121
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ModelState_AcquireInstance_MASM ENDP

RawrXD_ModelState_Initialize_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_122
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ModelState_Initialize_MASM ENDP

RawrXD_ModelState_Transition_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_123
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ModelState_Transition_MASM ENDP

RawrXD_NanoDisk_AbortJob_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_124
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_NanoDisk_AbortJob_MASM ENDP

RawrXD_NanoDisk_GetJobResult_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_125
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_NanoDisk_GetJobResult_MASM ENDP

RawrXD_NanoDisk_GetJobStatus_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_126
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_NanoDisk_GetJobStatus_MASM ENDP

RawrXD_NanoDisk_Init_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_127
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_NanoDisk_Init_MASM ENDP

RawrXD_NanoDisk_Shutdown_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_128
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_NanoDisk_Shutdown_MASM ENDP

RawrXD_NanoQuant_DequantizeMatMul_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_129
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_NanoQuant_DequantizeMatMul_MASM ENDP

RawrXD_NanoQuant_DequantizeTensor_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_130
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_NanoQuant_DequantizeTensor_MASM ENDP

RawrXD_NanoQuant_GetCompressionRatio_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_131
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_NanoQuant_GetCompressionRatio_MASM ENDP

RawrXD_NanoQuant_QuantizeTensor_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_132
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_NanoQuant_QuantizeTensor_MASM ENDP

RawrXD_NVMe_GetTemperature_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_133
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_NVMe_GetTemperature_MASM ENDP

RawrXD_NVMe_GetWearLevel_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_134
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_NVMe_GetWearLevel_MASM ENDP

RawrXD_Observable_Create_ActiveTextEditor_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_135
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Observable_Create_ActiveTextEditor_MASM ENDP

RawrXD_Observable_Create_VisibleTextEditors_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_136
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Observable_Create_VisibleTextEditors_MASM ENDP

RawrXD_Observable_Create_WorkspaceFolders_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_137
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Observable_Create_WorkspaceFolders_MASM ENDP

RawrXD_OrchestratorInitialize_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_138
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_OrchestratorInitialize_MASM ENDP

RawrXD_OutputChannel_Append_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_139
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_OutputChannel_Append_MASM ENDP

RawrXD_OutputChannel_AppendLine_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_140
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_OutputChannel_AppendLine_MASM ENDP

RawrXD_OutputChannel_Create_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_141
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_OutputChannel_Create_MASM ENDP

RawrXD_OutputChannel_CreateAPI_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_142
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_OutputChannel_CreateAPI_MASM ENDP

RawrXD_Path_Join_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_143
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Path_Join_MASM ENDP

RawrXD_Path_Join_PackageJson_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_144
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Path_Join_PackageJson_MASM ENDP

RawrXD_Phase1Initialize_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_145
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Phase1Initialize_MASM ENDP

RawrXD_Phase1LogMessage_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_146
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Phase1LogMessage_MASM ENDP

RawrXD_Phase2Initialize_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_147
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Phase2Initialize_MASM ENDP

RawrXD_Phase3Initialize_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_148
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Phase3Initialize_MASM ENDP

RawrXD_Phase4Initialize_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_149
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Phase4Initialize_MASM ENDP

RawrXD_Pipe_RunServer_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_150
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Pipe_RunServer_MASM ENDP

RawrXD_PrintU64_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_151
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_PrintU64_MASM ENDP

RawrXD_ProcessReceivedHeartbeat_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_152
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ProcessReceivedHeartbeat_MASM ENDP

RawrXD_ProcessSwarmQueue_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_153
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ProcessSwarmQueue_MASM ENDP

RawrXD_Provider_FromDocumentSelector_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_154
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Provider_FromDocumentSelector_MASM ENDP

RawrXD_Provider_Register_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_155
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Provider_Register_MASM ENDP

RawrXD_QueueInferenceJob_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_156
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_QueueInferenceJob_MASM ENDP

RawrXD_RaftEventLoop_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_157
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_RaftEventLoop_MASM ENDP

RawrXD_RawrXD_Calc_ContentLength_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_158
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_RawrXD_Calc_ContentLength_MASM ENDP

RawrXD_rawrxd_dispatch_cli_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_159
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_rawrxd_dispatch_cli_MASM ENDP

RawrXD_rawrxd_dispatch_command_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_160
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_rawrxd_dispatch_command_MASM ENDP

RawrXD_rawrxd_dispatch_feature_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_161
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_rawrxd_dispatch_feature_MASM ENDP

RawrXD_rawrxd_get_feature_count_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_162
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_rawrxd_get_feature_count_MASM ENDP

RawrXD_RawrXD_JSON_Stringify_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_163
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_RawrXD_JSON_Stringify_MASM ENDP

RawrXD_RawrXD_Marketplace_ResolveSymbol_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_164
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_RawrXD_Marketplace_ResolveSymbol_MASM ENDP

RawrXD_RawrXD_UI_Push_Notify_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_165
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_RawrXD_UI_Push_Notify_MASM ENDP

RawrXD_ReadTsc_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_166
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ReadTsc_MASM ENDP

RawrXD_Registry_CreateKey_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_167
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Registry_CreateKey_MASM ENDP

RawrXD_Registry_KeyExists_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_168
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Registry_KeyExists_MASM ENDP

RawrXD_Registry_SetDwordValue_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_169
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Registry_SetDwordValue_MASM ENDP

RawrXD_Registry_SetQwordValue_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_170
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Registry_SetQwordValue_MASM ENDP

RawrXD_Registry_SetStringValue_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_171
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Registry_SetStringValue_MASM ENDP

RawrXD_Release_CoT_Lock_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_172
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Release_CoT_Lock_MASM ENDP

RawrXD_Release_CoT_Lock_Shared_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_173
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Release_CoT_Lock_Shared_MASM ENDP

RawrXD_ResolveZonePointer_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_174
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ResolveZonePointer_MASM ENDP

RawrXD_RingBufferConsumer_Initialize_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_175
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_RingBufferConsumer_Initialize_MASM ENDP

RawrXD_RingBufferConsumer_Shutdown_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_176
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_RingBufferConsumer_Shutdown_MASM ENDP

RawrXD_RouteModelLoad_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_177
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_RouteModelLoad_MASM ENDP

RawrXD_Sample_Logits_TopP_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_178
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Sample_Logits_TopP_MASM ENDP

RawrXD_SemVer_Parse_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_179
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_SemVer_Parse_MASM ENDP

RawrXD_SemVer_ParseRange_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_180
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_SemVer_ParseRange_MASM ENDP

RawrXD_SemVer_Satisfies_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_181
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_SemVer_Satisfies_MASM ENDP

RawrXD_ShellInteg_CompleteCommand_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_182
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ShellInteg_CompleteCommand_MASM ENDP

RawrXD_ShellInteg_ExecuteCommand_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_183
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ShellInteg_ExecuteCommand_MASM ENDP

RawrXD_ShellInteg_GetCommandHistory_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_184
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ShellInteg_GetCommandHistory_MASM ENDP

RawrXD_ShellInteg_GetStats_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_185
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ShellInteg_GetStats_MASM ENDP

RawrXD_ShellInteg_IsAlive_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_186
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ShellInteg_IsAlive_MASM ENDP

RawrXD_Shield_AES_DecryptShim_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_187
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Shield_AES_DecryptShim_MASM ENDP

RawrXD_Shield_GenerateHWID_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_188
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Shield_GenerateHWID_MASM ENDP

RawrXD_Shield_TimingCheck_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_189
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Shield_TimingCheck_MASM ENDP

RawrXD_Shield_VerifyIntegrity_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_190
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Shield_VerifyIntegrity_MASM ENDP

RawrXD_SidecarMain_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_191
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_SidecarMain_MASM ENDP

RawrXD_Spinlock_Acquire_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_192
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Spinlock_Acquire_MASM ENDP

RawrXD_Spinlock_Release_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_193
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Spinlock_Release_MASM ENDP

RawrXD_StartPipeServer_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_194
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_StartPipeServer_MASM ENDP

RawrXD_StreamFormatter_WriteToken_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_195
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_StreamFormatter_WriteToken_MASM ENDP

RawrXD_StreamTensorByName_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_196
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_StreamTensorByName_MASM ENDP

RawrXD_SubmitInferenceRequest_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_197
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_SubmitInferenceRequest_MASM ENDP

RawrXD_SubmitTask_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_198
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_SubmitTask_MASM ENDP

RawrXD_Swarm_Initialize_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_199
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Swarm_Initialize_MASM ENDP

RawrXD_Swarm_SubmitJob_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_200
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Swarm_SubmitJob_MASM ENDP

RawrXD_SwarmTransportControl_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_201
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_SwarmTransportControl_MASM ENDP

RawrXD_System_InitializePrimitives_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_202
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_System_InitializePrimitives_MASM ENDP

RawrXD_Telemetry_SanitizeData_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_203
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Telemetry_SanitizeData_MASM ENDP

RawrXD_Titan_DirectStorage_Cleanup_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_204
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Titan_DirectStorage_Cleanup_MASM ENDP

RawrXD_Titan_GGML_Cleanup_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_205
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Titan_GGML_Cleanup_MASM ENDP

RawrXD_Titan_InferenceThread_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_206
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Titan_InferenceThread_MASM ENDP

RawrXD_Titan_Initialize_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_207
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Titan_Initialize_MASM ENDP

RawrXD_Titan_LoadModel_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_208
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Titan_LoadModel_MASM ENDP

RawrXD_Titan_RunInference_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_209
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Titan_RunInference_MASM ENDP

RawrXD_Titan_RunInferenceStep_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_210
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Titan_RunInferenceStep_MASM ENDP

RawrXD_Titan_Shutdown_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_211
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Titan_Shutdown_MASM ENDP

RawrXD_Titan_Stop_All_Streams_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_212
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Titan_Stop_All_Streams_MASM ENDP

RawrXD_Titan_SubmitPrompt_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_213
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Titan_SubmitPrompt_MASM ENDP

RawrXD_Titan_Vulkan_Cleanup_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_214
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Titan_Vulkan_Cleanup_MASM ENDP

RawrXD_Unlock_800B_Kernel_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_215
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Unlock_800B_Kernel_MASM ENDP

RawrXD_ValidateModelAlignment_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_216
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_ValidateModelAlignment_MASM ENDP

RawrXD_Vram_Allocate_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_217
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Vram_Allocate_MASM ENDP

RawrXD_Vram_Initialize_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_218
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Vram_Initialize_MASM ENDP

RawrXD_VulkanDMA_RegisterTensor_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_219
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_VulkanDMA_RegisterTensor_MASM ENDP

RawrXD_VulkanKernel_AllocBuffer_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_220
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_VulkanKernel_AllocBuffer_MASM ENDP

RawrXD_VulkanKernel_Cleanup_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_221
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_VulkanKernel_Cleanup_MASM ENDP

RawrXD_VulkanKernel_CopyToDevice_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_222
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_VulkanKernel_CopyToDevice_MASM ENDP

RawrXD_VulkanKernel_CopyToHost_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_223
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_VulkanKernel_CopyToHost_MASM ENDP

RawrXD_VulkanKernel_CreatePipeline_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_224
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_VulkanKernel_CreatePipeline_MASM ENDP

RawrXD_VulkanKernel_DispatchFlashAttn_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_225
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_VulkanKernel_DispatchFlashAttn_MASM ENDP

RawrXD_VulkanKernel_DispatchMatMul_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_226
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_VulkanKernel_DispatchMatMul_MASM ENDP

RawrXD_VulkanKernel_GetStats_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_227
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_VulkanKernel_GetStats_MASM ENDP

RawrXD_VulkanKernel_HotswapShader_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_228
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_VulkanKernel_HotswapShader_MASM ENDP

RawrXD_VulkanKernel_Init_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_229
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_VulkanKernel_Init_MASM ENDP

RawrXD_VulkanKernel_LoadShader_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_230
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_VulkanKernel_LoadShader_MASM ENDP

RawrXD_WebviewPanel_CreateAPI_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_231
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_WebviewPanel_CreateAPI_MASM ENDP

RawrXD_Week1Initialize_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_232
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Week1Initialize_MASM ENDP

RawrXD_Week23Initialize_MASM PROC
    sub rsp, 28h
    lea rcx, sz_stub_233
    call OutputDebugStringA
    add rsp, 28h
    ret
RawrXD_Week23Initialize_MASM ENDP

RawrXD_ArenaAllocate_MASM PROC
    push rbx
    sub rsp, 20h
    mov rbx, rcx
    lea rcx, sz_stub_arena_allocate
    call OutputDebugStringA
    mov rcx, rbx
    call malloc
    add rsp, 20h
    pop rbx
    ret
RawrXD_ArenaAllocate_MASM ENDP

END
