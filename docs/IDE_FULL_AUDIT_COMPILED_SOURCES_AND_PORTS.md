# RawrXD IDE — Full Audit: Compiled Sources and Asset/Port Inventory

**Purpose:** Unsummarized, exhaustive inventory of (1) everything that is compiled into the IDE and related binaries, and (2) all network ports and asset/resource paths. No summarization; full enumeration for compliance and port documentation.

**Generated:** 2026-02-15. **Source of truth:** `CMakeLists.txt`, `src/main.cpp`, `src/complete_server.cpp`, `src/tool_server.cpp`, Win32IDE and related sources.

---

## 1. Build Targets (Executables and Libraries)

| Target | Type | Output | Description |
|--------|------|--------|-------------|
| RawrEngine | Executable | RawrEngine.exe | CLI inference engine + agentic core; uses SOURCES + ASM_KERNEL_SOURCES. Default port 8080 when run as server. |
| RawrXD_Gold | Executable | RawrXD_Gold.exe | Standalone monolithic deployment; SOURCES + ASM_KERNEL_SOURCES + Gold-only sources. |
| RawrXD-InferenceEngine | Executable | RawrXD-InferenceEngine.exe | Standalone inference (GGUF load, emit tokens); INFERENCE_ENGINE_SOURCES + INFERENCE_ASM_SOURCES. |
| rawrxd-monaco-gen | Executable | rawrxd-monaco-gen.exe | Monaco/React IDE generator; monaco_gen.cpp, react_ide_generator.cpp. |
| RawrXD-Win32IDE | Executable | RawrXD-Win32IDE.exe | Full Win32 GUI IDE; WIN32IDE_SOURCES + ASM_KERNEL_SOURCES + RawrXD-Win32IDE.rc. |
| RawrXD-Gold | Executable | RawrXD-Gold.exe (WIN32) | Gold IDE variant; GOLD_SOURCES + ASM_KERNEL_SOURCES. |
| RawrXD_CLI | (Built via RawrEngine or separate) | RawrXD_CLI.exe | Pure CLI; default port 23959. See README / main.cpp. |
| RawrXD_NanoQuant | Static library | (linked into IDE/Inference when present) | Optional NanoQuant. |
| RawrXD_MultiWindow_Kernel | Shared library | RawrXD_MultiWindow_Kernel.dll | MASM64 + multiwindow_scheduler.cpp; when RAWR_HAS_MASM. |
| RawrXD_DynamicPromptEngine | Shared library | RawrXD_DynamicPromptEngine.dll | MASM64 + dynamic_prompt_engine_glue.cpp; when RAWR_HAS_MASM. |
| RawrXD-RE-Library | Library | (from src/reverse_engineering) | PE/RE tools; linked by RawrEngine, RawrXD_Gold, RawrXD-Win32IDE. |
| quickjs_static | Static library | (linked into Win32IDE when 3rdparty/quickjs present) | QuickJS; else RAWR_QUICKJS_STUB=1. |

---

## 2. ASM Kernel Sources (MASM64 — All Compiled When RAWR_HAS_MASM)

Every file below is compiled with `ASM_MASM` and linked into RawrEngine, RawrXD_Gold, and (where applicable) RawrXD-Win32IDE.

| Path | Notes |
|------|--------|
| src/asm/inference_core.asm | |
| src/asm/FlashAttention_AVX512.asm | |
| src/asm/quant_avx2.asm | |
| src/asm/RawrXD_KQuant_Dequant.asm | |
| src/asm/request_patch.asm | |
| src/asm/inference_kernels.asm | |
| src/asm/model_bridge_x64.asm | |
| src/asm/RawrXD_DualAgent_Orchestrator.asm | |
| src/asm/RawrXD_DiskRecoveryAgent.asm | |
| src/asm/disk_recovery_scsi.asm | |
| src/asm/RawrXD-AnalyzerDistiller.asm | |
| src/asm/RawrXD-StreamingOrchestrator.asm | |
| src/asm/RawrXD_Telemetry_Kernel.asm | |
| src/asm/RawrXD_Prometheus_Exporter.asm | |
| src/asm/RawrXD_SelfPatch_Agent.asm | |
| src/asm/RawrXD_SourceEdit_Kernel.asm | |
| src/asm/feature_dispatch_bridge.asm | |
| src/asm/gui_dispatch_bridge.asm | |
| src/asm/RawrXD_CopilotGapCloser.asm | |
| src/asm/native_speed_kernels.asm | |
| src/asm/DirectML_Bridge.asm | |
| src/asm/RawrXD_Hotpatch_Kernel.asm | |
| src/asm/RawrXD_Snapshot.asm | |
| src/asm/RawrXD_Pyre_Compute.asm | |
| src/asm/vision_projection_kernel.asm | |
| src/asm/quantum_beaconism_backend.asm | |
| src/asm/RawrXD_DualEngine_QuantumBeacon.asm | |
| src/asm/RawrXD_DualEngine_Pure.asm | |
| src/asm/RawrXD_OverclockGov_Pure.asm | |
| src/asm/RawrXD_AuditSystem_Pure.asm | |
| src/asm/Advanced_Code_Deobfuscator.asm | |
| src/asm/Code_Pattern_Reconstructor.asm | |

Excluded (commented in CMake): memory_patch.asm, byte_search.asm, RawrXD_Complete_ReverseEngineered.asm.

Win32IDE additionally links (when MASM): RawrXD_DualEngine_QuantumBeacon.asm, RawrXD_UnifiedOverclock_Governor.asm, quantum_beaconism_backend.asm (duplicated in WIN32IDE_SOURCES). MultiWindow Kernel: RawrXD_MultiWindow_Kernel.asm. DynamicPromptEngine: RawrXD_DynamicPromptEngine.asm.

---

## 3. Core SOURCES (RawrEngine / RawrXD_Gold)

The following list is the full `SOURCES` set from the root `CMakeLists.txt`. Every entry is a file that is compiled into RawrEngine and RawrXD_Gold.

- src/main.cpp
- src/stubs.cpp
- src/memory_core.cpp
- src/hot_patcher.cpp
- src/cpu_inference_engine.cpp
- src/complete_server.cpp
- src/agentic_engine.cpp
- src/subagent_core.cpp
- src/agent_history.cpp
- src/agent_policy.cpp
- src/agent_explainability.cpp
- src/streaming_gguf_loader.cpp
- src/utils/Diagnostics.cpp
- src/gguf_loader.cpp
- src/gguf_vocab_resolver.cpp
- src/compression_interface.cpp
- src/engine/gguf_core.cpp
- src/engine/inference_kernels.cpp
- src/engine/transformer.cpp
- src/engine/bpe_tokenizer.cpp
- src/engine/sampler.cpp
- src/engine/rawr_engine.cpp
- src/engine/core_generator.cpp
- src/engine/pyre_compute.cpp
- src/engine/sovereign_engines.cpp
- src/codec/compression.cpp
- src/codec/brutal_gzip.cpp
- src/core/execution_scheduler.cpp
- src/cli/agentic_decision_tree.cpp
- src/cli/cli_autonomy_loop.cpp
- src/cli/deep_iteration_engine.cpp
- src/cli/cli_headless_systems.cpp
- src/core/agent_safety_contract.cpp
- src/core/confidence_gate.cpp
- src/core/deterministic_replay.cpp
- src/core/execution_governor.cpp
- src/core/multi_response_engine.cpp
- src/core/streaming_engine_registry.cpp
- src/core/flash_attention.cpp
- src/core/gpu_backend_bridge.cpp
- src/core/native_speed_layer.cpp
- src/core/local_ai_core.cpp
- src/core/native_inference_pipeline.cpp
- src/core/layer_offload_manager.cpp
- src/core/model_memory_hotpatch.cpp
- src/core/byte_level_hotpatcher.cpp
- src/core/memory_patch_byte_search_stubs.cpp
- src/core/unified_hotpatch_manager.cpp
- src/core/proxy_hotpatcher.cpp
- src/core/model_bruteforce_engine.cpp
- src/server/gguf_server_hotpatch.cpp
- src/agent/agentic_hotpatch_orchestrator.cpp
- src/core/agentic_task_graph.cpp
- src/core/embedding_engine.cpp
- src/core/vision_encoder.cpp
- src/core/vision_token_gate.cpp
- src/core/vision_embedding_cache.cpp
- src/core/vision_kv_isolation.cpp
- src/core/vision_quantized_encoder.cpp
- src/core/vision_gpu_staging.cpp
- src/marketplace/extension_marketplace.cpp
- src/auth/rbac_engine.cpp
- src/core/shadow_page_detour.cpp
- src/core/sentinel_watchdog.cpp
- src/agentic_loop_state.cpp
- src/agent/autonomous_subagent.cpp
- src/agent/agentic_failure_detector.cpp
- src/agent/agentic_puppeteer.cpp
- src/agent/llm_http_client.cpp
- src/agent/model_invoker.cpp
- src/agent/planner.cpp
- src/agent/eval_framework.cpp
- src/agent/local_reasoning_engine.cpp
- src/agent/agent_self_repair.cpp
- src/agent/agent_self_healing_orchestrator.cpp
- src/agentic/DeterministicReplayEngine.cpp
- src/agentic/AgentToolHandlers.cpp
- src/agentic/autonomous_recovery_orchestrator.cpp
- src/agentic/AgentOrchestrator.cpp
- src/agentic/OrchestratorBridge.cpp
- src/agentic/AgenticNavigator.cpp
- src/agentic/RawrXD_ToolRegistry.cpp
- src/agentic/ToolRegistry.cpp
- src/agentic/RawrXD_AgentLoop.cpp
- src/agentic/agentic_executor.cpp
- src/core/auto_repair_orchestrator.cpp
- src/core/agent_memory_indexer.cpp
- src/agent/DiskRecoveryAgent.cpp
- src/core/swarm_decision_bridge.cpp
- src/core/universal_model_hotpatcher.cpp
- src/core/production_release.cpp
- src/core/webrtc_signaling.cpp
- src/core/gpu_kernel_autotuner.cpp
- src/core/sandbox_integration.cpp
- src/core/amd_gpu_accelerator.cpp
- src/core/intel_gpu_accelerator.cpp
- src/core/arm64_gpu_accelerator.cpp
- src/core/cerebras_wse_accelerator.cpp
- src/core/accelerator_router.cpp
- src/core/pdb_gsi_hash.cpp
- src/core/pdb_native.cpp
- src/core/pdb_reference_provider.cpp
- src/core/enterprise_license.cpp
- src/core/enterprise_license_stubs.cpp
- src/core/enterprise_licensev2_impl.cpp
- src/core/camellia256_bridge.cpp
- src/core/swarm_coordinator.cpp
- src/core/swarm_worker.cpp
- src/core/swarm_network_stubs.cpp
- src/tool_registry.cpp
- src/license_enforcement.cpp
- src/feature_flags_runtime.cpp
- src/tools/license_validator_manifest.cpp
- src/core/agentic_autonomous_config.cpp
- src/core/enterprise_feature_manager.cpp
- src/model_name_utils.cpp
- src/core/multi_gpu.cpp
- src/core/engine_registry.cpp
- src/agent/telemetry_collector.cpp
- src/agentic_observability.cpp
- src/agentic/BoundedAgentLoop.cpp
- src/agentic/FIMPromptBuilder.cpp
- src/core/rawrxd_subsystem_api.cpp
- src/config/IDEConfig.cpp
- src/agentic/DiskRecoveryAgent.cpp
- src/agentic/DiskRecoveryToolHandler.cpp
- src/win32app/agentic_bridge_headless.cpp
- src/core/ai_agent_masm_stubs.cpp
- src/core/context_deterioration_hotpatch.cpp
- src/lsp/lsp_hotpatch_bridge.cpp
- src/lsp/hotpatch_symbol_provider.cpp
- src/core/license_anti_tampering.cpp
- src/win32app/reverse_engineered_stubs.cpp
- src/core/support_tier.cpp
- src/core/subsystem_mode_stubs.cpp
- src/core/feature_registry.cpp
- src/core/menu_auditor.cpp
- src/lsp/gguf_diagnostic_provider.cpp
- src/lsp/RawrXD_LSPServer.cpp
- src/core/extension_polyfill_engine.cpp
- src/modules/vscode_extension_api.cpp
- src/core/js_extension_host.cpp
- src/model_source_resolver.cpp
- src/modules/ReverseEngineering.cpp
- src/agent/agentic_deep_thinking_engine_stub.cpp
- src/win32app/Win32IDE_logMessage_stub.cpp
- src/win32app/Win32IDE_headless_stubs.cpp
- src/core/analyzer_distiller_stubs.cpp
- src/core/streaming_orchestrator_stubs.cpp
- src/core/pt_driver_contract.cpp
- src/cli/swarm_orchestrator.cpp
- src/core/voice_chat.cpp
- src/core/voice_automation.cpp
- src/core/directml_compute.cpp
- src/core/gguf_dml_bridge.cpp
- src/core/dml_streaming_integration.cpp
- src/dml_inference_engine.cpp
- src/core/watchdog_service.cpp
- src/core/shortcut_manager.cpp
- src/core/backup_manager.cpp
- src/core/alert_system.cpp
- src/core/feature_registration.cpp
- src/core/feature_handlers.cpp
- src/core/auto_feature_registry.cpp
- src/core/intent_engine.cpp
- src/core/missing_handler_stubs.cpp
- src/core/model_training_pipeline.cpp
- src/core/ssot_handlers.cpp
- src/core/ssot_handlers_ext.cpp
- src/core/ssot_validation.cpp
- src/core/unified_command_dispatch.cpp
- src/core/chain_of_thought_engine.cpp
- src/core/reasoning_profile.cpp
- src/core/reasoning_pipeline_orchestrator.cpp
- src/core/reasoning_cot_bridge.cpp
- src/core/rawrxd_state_mmf.cpp
- src/vulkan_compute.cpp
- src/core/live_binary_patcher.cpp
- src/core/traversal_strategy.cpp
- src/core/layer_contribution_scorer.cpp
- src/core/semantic_delta_tracker.cpp
- src/core/cross_run_tensor_cache.cpp
- src/core/convergence_controller.cpp
- src/core/iterative_tensor_traversal.cpp
- src/core/sqlite_wrapper.cpp
- src/core/sqlite3.c
- src/core/thread_pool.cpp
- src/core/memory_pressure_handler.cpp
- src/core/jsonrpc_parser.cpp
- src/agentic/multi_file_transaction.cpp
- src/core/transaction_journal.cpp
- src/agentic/agentic_transaction.cpp
- src/core/vector_index.cpp
- src/agentic/model_cascade.cpp
- src/agentic/context_assembler.cpp
- src/ui/chat_message_renderer.cpp
- src/ui/tool_action_status.cpp
- src/ui/chat_panel.cpp
- src/core/crash_containment.cpp
- src/core/patch_rollback_ledger.cpp
- src/core/quant_hysteresis.cpp
- src/core/masm_stress_harness.cpp
- src/core/perf_telemetry.cpp
- src/core/convergence_stress_harness.cpp
- src/git/git_context.cpp
- src/lsp/diagnostic_consumer.cpp
- src/core/prompt_template_engine.cpp
- src/gpu/speculative_decoder_v2.cpp
- src/inference/ultra_fast_inference.cpp
- src/telemetry/completion_feedback.cpp
- src/telemetry/replay_telemetry_fusion.cpp
- src/telemetry/hotpatch_telemetry_safety.cpp
- src/telemetry/UnifiedTelemetryCore.cpp
- src/gpu/gpu_backend.cpp
- src/gpu/kv_cache_optimizer.cpp
- src/gpu/speculative_decoder.cpp
- src/agentic_controller.cpp
- src/ui/phase2_integration_example.cpp
- src/agentic_agent_coordinator.cpp
- src/production_config_manager.cpp
- src/core/autonomous_workflow_engine.cpp
- src/core/workspace_reasoning_profiles.cpp
- src/core/deterministic_swarm.cpp
- src/core/safe_refactor_engine.cpp
- src/core/reasoning_schema_versioning.cpp
- src/core/cot_fallback_system.cpp
- src/core/input_guard_slicer.cpp
- src/test_harness/replay_fixture.cpp
- src/test_harness/replay_oracle.cpp
- src/test_harness/replay_mock_inference.cpp
- src/test_harness/replay_harness.cpp
- src/test_harness/replay_reporter.cpp
- src/core/instructions_provider.cpp
- src/core/distributed_pipeline_orchestrator.cpp
- src/core/hotpatch_control_plane.cpp
- src/core/static_analysis_engine.cpp
- src/core/problems_aggregator.cpp
- src/security/secrets_scanner.cpp
- src/security/dependency_audit.cpp
- src/security/sast_rule_engine.cpp
- src/security/RawrXD_GoogleDork_Scanner.cpp
- src/security/RawrXD_Universal_Dorker.cpp
- src/core/code_linter.cpp
- src/core/semantic_code_intelligence.cpp
- src/core/enterprise_telemetry_compliance.cpp
- src/core/update_signature.cpp
- src/core/cursor_github_parity_bridge.cpp
- src/core/local_parity_bridge.cpp
- src/core/swarm_reconciliation.cpp
- src/core/quickjs_sandbox.cpp
- src/core/plugin_signature.cpp
- src/core/enterprise_stress_tests.cpp
- src/core/auto_update_system.cpp
- src/agentic/autonomous_verification_loop.cpp
- src/agentic/autonomous_background_daemon.cpp
- src/core/knowledge_graph_core.cpp
- src/core/swarm_conflict_resolver.cpp
- src/core/autonomous_debugger.cpp
- src/agentic/autonomous_communicator.cpp
- src/telemetry/telemetry_export.cpp
- src/agentic/agentic_composer_ux.cpp
- src/context/context_mention_parser.cpp
- src/multimodal_engine/vision_encoder.cpp
- src/ide/refactoring_plugin.cpp
- src/ide/language_plugin.cpp
- src/context/semantic_index.cpp
- src/ide/resource_generator.cpp
- src/ai/codebase_rag.cpp
- src/core/self_host_engine.cpp
- src/core/hardware_synthesizer.cpp
- src/core/mesh_brain.cpp
- src/core/mesh_brain_asm_stubs.cpp
- src/core/speciator_engine.cpp
- src/core/neural_bridge.cpp
- src/core/omega_orchestrator.cpp
- src/core/transcendence_coordinator.cpp
- src/win32app/Win32IDE_TranscendencePanel.cpp

RawrEngine additionally gets: src/core/native_debugger_engine.cpp, src/core/native_debugger_symbols.cpp, src/agentic/AgentOllamaClient.cpp (target_sources).

---

## 4. WIN32IDE_SOURCES (RawrXD-Win32IDE — Full List)

RawrXD-Win32IDE is built from WIN32IDE_SOURCES + ASM_KERNEL_SOURCES + src/win32app/RawrXD-Win32IDE.rc. WIN32IDE_SOURCES includes all of the following (exact list from CMakeLists.txt lines 1662–2255). Only the file paths are enumerated; headers are omitted where they are not compiled as part of the target.

- src/win32app/main_win32.cpp
- src/win32app/Win32IDE.cpp
- src/win32app/Win32IDE_Core.cpp
- src/win32app/Win32IDE_Sidebar.cpp
- src/win32app/Win32IDE_VSCodeUI.cpp
- src/win32app/Win32IDE_NativePipeline.cpp
- src/win32app/Win32IDE_PowerShellPanel.cpp
- src/win32app/Win32IDE_PowerShell.cpp
- src/win32app/Win32IDE_Logger.cpp
- src/win32app/Win32IDE_FileOps.cpp
- src/win32app/Win32IDE_Commands.cpp
- src/win32app/Win32IDE_Debugger.cpp
- src/win32app/Win32IDE_AgenticBridge.cpp
- src/win32app/Win32IDE_AgentCommands.cpp
- src/win32app/plan_mode_handler.cpp
- src/win32app/Win32IDE_Autonomy.cpp
- src/win32app/Win32IDE_SyntaxHighlight.cpp
- src/win32app/Win32IDE_Themes.cpp
- src/win32app/Win32IDE_Annotations.cpp
- src/win32app/Win32IDE_Session.cpp
- src/win32app/Win32IDE_StreamingUX.cpp
- src/win32app/Win32IDE_ReverseEngineering.cpp
- src/win32app/Win32IDE_DecompilerView.cpp
- src/win32app/Win32IDE_FeatureManifest.cpp
- src/win32app/Win32IDE_GhostText.cpp
- src/win32app/Win32IDE_OutlinePanel.cpp
- src/win32app/Win32IDE_RenamePreview.cpp
- src/win32app/Win32IDE_Tier2Cosmetics.cpp
- src/win32app/Win32IDE_Tier1Cosmetics.cpp
- src/win32app/Win32IDE_Breadcrumbs.cpp
- src/win32app/Win32IDE_PlanExecutor.cpp
- src/win32app/Win32IDE_FailureDetector.cpp
- src/win32app/Win32IDE_FailureIntelligence.cpp
- src/win32app/Win32IDE_Settings.cpp
- src/win32app/Win32IDE_LocalServer.cpp
- src/win32app/Win32IDE_BackendSwitcher.cpp
- src/win32app/Win32IDE_LLMRouter.cpp
- src/win32app/Win32IDE_LSPClient.cpp
- src/win32app/Win32IDE_AsmSemantic.cpp
- src/win32app/Win32IDE_LSP_AI_Bridge.cpp
- src/win32app/Win32IDE_MultiResponse.cpp
- src/win32app/Win32IDE_ExecutionGovernor.cpp
- src/win32app/Win32IDE_SubAgent.cpp
- src/win32app/Win32IDE_AgentHistory.cpp
- src/win32app/Win32IDE_AgentPanel.cpp
- src/win32app/Win32IDE_DiskRecovery.cpp
- src/win32app/Win32TerminalManager.cpp
- src/win32app/TransparentRenderer.cpp
- src/plugin_system/win32_plugin_loader.cpp
- src/win32app/Win32IDE_Plugins.cpp
- src/config/IDEConfig.cpp
- (plus all engine/agentic/core/lsp/ui/asm modules listed in WIN32IDE_SOURCES block: stubs, AgentToolHandlers, BoundedAgentLoop, OllamaProvider, DiffEngine, DeterministicReplayEngine, autonomous_recovery_orchestrator, AgentOrchestrator, OrchestratorBridge, AgenticNavigator, RawrXD_ToolRegistry, ToolRegistry, tool_registry, agentic_executor, FIMPromptBuilder, agentic_observability, rawrxd_subsystem_api, subsystem_mode_stubs, local_parity_bridge, layer_offload_manager, runtime_core, telemetry_collector, reverse_engineered_stubs, auto_repair_orchestrator, agent_memory_indexer, agentic_transaction, mesh_brain_asm_stubs, DiskRecoveryAgent, DiskRecoveryToolHandler, agent_history, agent_policy, agent_explainability, ErrorReporter, engine_manager, cpu_inference_engine, memory_core, hot_patcher, streaming_gguf_loader, Diagnostics, gguf_loader, gguf_vocab_resolver, model_source_resolver, compression_interface, gguf_core, inference_kernels, transformer, bpe_tokenizer, sampler, rawr_engine, core_generator, pyre_compute, sovereign_engines, compression, brutal_gzip, streaming_engine_registry, gpu_backend_bridge, enterprise_license, enterprise_licensev2_impl, license_validator_manifest, license_anti_tampering, camellia256_bridge, watchdog_service, multi_response_engine, flash_attention, execution_scheduler, execution_governor, multi_gpu, agent_safety_contract, deterministic_replay, confidence_gate, SwarmPanel, DualAgentPanel, native_debugger_engine, native_debugger_symbols, NativeDebugPanel, model_memory_hotpatch, byte_level_hotpatcher, memory_patch_byte_search_stubs, unified_hotpatch_manager, proxy_hotpatcher, model_bruteforce_engine, gguf_server_hotpatch, pt_driver_contract, shadow_page_detour, sentinel_watchdog, HotpatchPanel, agentic_hotpatch_orchestrator, llm_http_client, model_invoker, planner, eval_framework, autonomous_subagent, agentic_failure_detector, ai_agent_masm_stubs, agentic_puppeteer, local_reasoning_engine, agent_self_repair, agent_self_healing_orchestrator, vsix_loader_win32, codex_ultimate, vscode_extension_api, Win32IDE_VSCodeExtAPI, swarm_decision_bridge, universal_model_hotpatcher, production_release, quantum_safe_transport, adaptive_pipeline_parallel, universal_model_merger, webrtc_signaling, gpu_kernel_autotuner, sandbox_integration, amd_gpu_accelerator, intel_gpu_accelerator, arm64_gpu_accelerator, cerebras_wse_accelerator, accelerator_router, pdb_gsi_hash, pdb_reference_provider, Win32IDE_WebView2, Win32IDE_MonacoThemes, RawrXD_LSPServer, Win32IDE_LSPServer, lsp_hotpatch_bridge, hotpatch_symbol_provider, gguf_diagnostic_provider, MonacoCoreEngine, monaco_core_stubs, WebView2EditorEngine, RichEditEditorEngine, EditorEngineFactory, Win32IDE_EditorEngine, pdb_native, pdb_lsp_bridge, Win32IDE_PDBSymbols, feature_registry, menu_auditor, auto_discovery, Win32IDE_AuditDashboard, final_gauntlet, Win32IDE_Gauntlet, voice_chat, Win32IDE_VoiceChat, voice_automation, Win32IDE_VoiceAutomation, shortcut_manager, backup_manager, alert_system, Win32IDE_QuickWins, feature_registration, feature_handlers, intent_engine, missing_handler_stubs, model_training_pipeline, AgentOllamaClient, ssot_handlers, ssot_handlers_ext, ssot_validation, unified_command_dispatch, swarm_orchestrator, chain_of_thought_engine, Win32IDE_Telemetry, mcp_integration, Win32IDE_MCP, Win32IDE_FlightRecorder, quickjs_extension_host, quickjs_node_shims, quickjs_vscode_bindings, rawrxd_state_mmf, js_extension_host, extension_polyfill_engine, vulkan_compute, live_binary_patcher, autonomous_workflow_engine, workspace_reasoning_profiles, reasoning_profile, deterministic_swarm, reasoning_schema_versioning, cot_fallback_system, input_guard_slicer, safe_refactor_engine, agentic_agent_coordinator, production_config_manager, agentic_task_graph, embedding_engine, vector_index, vision_encoder, vision_token_gate, vision_embedding_cache, vision_kv_isolation, vision_quantized_encoder, vision_gpu_staging, extension_marketplace, rbac_engine, agentic_loop_state, transaction_journal, instructions_provider, Win32IDE_Instructions, unity_engine_integration, unreal_engine_integration, game_engine_manager, Win32IDE_GameEnginePanel, crucible_engine, Win32IDE_CruciblePanel, copilot_gap_closer, Win32IDE_CopilotGapPanel, distributed_pipeline_orchestrator, Win32IDE_PipelinePanel, hotpatch_control_plane, Win32IDE_HotpatchCtrlPanel, static_analysis_engine, code_linter, diagnostic_consumer, Win32IDE_StaticAnalysisPanel, Win32IDE_ProblemsPanel, Win32IDE_BuildRunner, Win32IDE_SecurityScans, problems_panel_bridge, Win32IDE_AgentStreamingBridge, semantic_code_intelligence, Win32IDE_SemanticPanel, enterprise_telemetry_compliance, Win32IDE_TelemetryPanel, Win32IDE_Tier3Polish, Win32IDE_Tier3Cosmetics, Win32IDE_Tier5Cosmetics, Win32IDE_CrashReporter, Win32IDE_ColorPicker, Win32IDE_EmojiSupport, Win32IDE_ShortcutEditor, Win32IDE_TelemetryDashboard, Win32IDE_MarketplacePanel, Win32IDE_TestExplorerTree, Win32IDE_NetworkPanel, omega_orchestrator, mesh_brain, mesh_brain_asm_stubs, speciator_engine, neural_bridge, self_host_engine, hardware_synthesizer, transcendence_coordinator, Win32IDE_TranscendencePanel, VulkanRenderer, OSExplorerInterceptor, Win32IDE_MCPHooks, TodoManager, IocpFileWatcher, IDEDiagnosticAutoHealer, ConsentPrompt, AutonomousAgent, chat_message_renderer, tool_action_status, chat_panel, crash_containment, patch_rollback_ledger, quant_hysteresis, masm_stress_harness, perf_telemetry, convergence_stress_harness, update_signature, cursor_github_parity_bridge, swarm_reconciliation, quickjs_sandbox, plugin_signature, enterprise_stress_tests, auto_update_system, autonomous_verification_loop, autonomous_background_daemon, knowledge_graph_core, swarm_conflict_resolver, autonomous_debugger, autonomous_communicator, UnifiedTelemetryCore, telemetry_export, agentic_composer_ux, context_mention_parser, vision_encoder (multimodal), refactoring_plugin, language_plugin, semantic_index, resource_generator, Win32IDE_CursorParity, Win32IDE_GUILayoutHotpatch, Win32IDE_ProvableAgent, Win32IDE_AIReverseEngineering, Win32IDE_AirgappedEnterprise, Win32IDE_FlagshipFeatures, project_context, universal_model_router, model_registry, model_name_utils, checkpoint_manager, agentic_autonomous_config, enterprise_feature_manager, context_deterioration_hotpatch, interpretability_panel, benchmark_menu_stub, benchmark_runner_stub, feature_registry_panel, monaco_settings_dialog, RAWRXD_ThermalDashboard, VSCodeMarketplaceAPI, multi_file_search_stub, Win32IDE_LicenseCreator, license_enforcement, feature_flags_runtime, RawrXD_DualEngine_QuantumBeacon.asm, RawrXD_UnifiedOverclock_Governor.asm, quantum_beaconism_backend.asm

Exact file count: see CMakeLists.txt set(WIN32IDE_SOURCES ...) from line 1662 to 2255.

---

## 5. Network Ports — Full Inventory

| Port | Default | Override | Where used | Purpose |
|------|---------|----------|------------|---------|
| 23959 | Yes (RawrXD_CLI) | `--port` (main.cpp) | src/main.cpp | HTTP server for CLI; Win32 IDE connects for /api/chat and /api/tool. |
| 8080 | Yes (RawrEngine as server) | `--port` | src/main.cpp | RawrEngine default HTTP server port when run as server. |
| 11434 | Yes | OLLAMA_HOST (host:port) | Win32IDE.cpp, main.cpp, tool_server.cpp, Win32IDE_WebView2.cpp | Ollama API (default localhost:11434). |
| 11435 | Yes (tool server) | `--port` | src/tool_server.cpp | SimpleHTTPServer (tool server); if same as Ollama, code uses 11435 to avoid loop. |
| 11437 | (example) | — | Win32IDE_SwarmPanel.cpp | Swarm leader port example (IP:PORT e.g. 192.168.1.100:11437). |
| 3478 | STUN default | — | src/core/webrtc_signaling.cpp | STUN server default port. |
| 8080 | (Cursor parity) | — | Win32IDE_CursorParity.cpp | params["port"] = "8080" for local server. |
| (configurable) | — | server.port / localServerPort | vscode_extension_api.cpp | getHost()->getSettingsMut().localServerPort. |

**Summary:** Primary IDE↔CLI server port is **23959**. Ollama is **11434**. Tool server is **11435**. RawrEngine server default is **8080**. STUN **3478**. Swarm uses configurable IP:PORT (e.g. 11437).

---

## 6. Asset and Resource Paths

| Asset / resource | Path or origin | Notes |
|------------------|----------------|--------|
| App manifest | RawrXD-Win32IDE.manifest (embedded via .rc) | 1 RT_MANIFEST in RawrXD-Win32IDE.rc. |
| Version info | RawrXD-Win32IDE.rc | FILEVERSION/PRODUCTVERSION 7.4.0.0. |
| Other .rc | gui/RawrXD_Titan_GUI.rc | Separate GUI resource script. |
| QuickJS | 3rdparty/quickjs/ | quickjs.c, libregexp.c, libunicode.c, cutils.c, quickjs-libc.c, libbf.c; optional; stub if missing. |
| LSP/config paths | (IDE settings) | LSP config file path from getLSPConfigFilePath(); see Win32IDE_Commands. |
| settings.json | (app data or workspace) | Pure JSON settings; see README / IDEConfig. |
| .rawrxd/ | Workspace | RAG index, metrics, etc. (e.g. .rawrxd/rag_index.bin, .rawrxd/metrics.prom). |
| data/cve_database.json | (optional) | dependency_audit; CVE lookup. |
| tools.instructions.md | (project) | instructions_provider; read by HTTP+CLI+GUI. |
| GetModuleFileName | Win32 | Used to resolve exe directory for relative paths (various). |

No embedded binary assets (icons, images) are listed in the main IDE .rc; only manifest and version.

---

## 7. Ship and Other Compiled Artifacts

- **Ship/** — Contains ASM and C++ used by other builds (e.g. RawrXD_Universal_Dorker.asm, DorkScanner MASM). Not all Ship sources are in the default IDE build; see Ship/CMakeLists.txt and root CMakeLists for which Ship targets are included.
- **RawrXD-InferenceEngine** — INFERENCE_ENGINE_SOURCES (main inference entry, gguf load, etc.) + INFERENCE_ASM_SOURCES (inference_core, inference_kernels, quant_avx2, KQuant_Dequant, KQuant_Kernel, FlashAttention_AVX512, Telemetry_Kernel, Pyre_Compute).

---

## 8. Definitions That Affect What Is Compiled

- RAWR_HAS_MASM=1 — Enables ASM_MASM and all ASM_KERNEL_SOURCES; MultiWindow and DynamicPromptEngine DLLs when MASM.
- RAWR_QUICKJS_STUB=1 — When 3rdparty/quickjs is missing; JS extension host uses stubs.
- RAWR_HAS_VULKAN=1 — Vulkan linked into Win32IDE when Vulkan_FOUND or _VK_SDK_DIRS.
- RAWRXD_HAS_MW_KERNEL=1 — When RawrXD_MultiWindow_Kernel target is built.
- RAWRXD_HAS_PROMPT_ENGINE=1 — When RawrXD_DynamicPromptEngine target is built.
- RAWR_HAS_NANOQUANT=1 — When RawrXD_NanoQuant target is built and linked.

This document is the full unsummarized audit of compiled sources and asset/port usage for the RawrXD IDE and related binaries.
