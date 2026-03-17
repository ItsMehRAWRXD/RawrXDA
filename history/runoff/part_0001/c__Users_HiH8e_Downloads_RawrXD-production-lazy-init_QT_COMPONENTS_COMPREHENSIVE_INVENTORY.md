# RawrXD Qt UI Framework & IDE Components - Comprehensive Inventory

**Last Updated**: December 28, 2025  
**Repository**: RawrXD-production-lazy-init  
**Framework**: Qt6 with C++20  
**Build System**: CMake 3.20+

---

## Executive Summary

The RawrXD IDE contains **180+ C++ source files** in the `src/qtapp` directory totaling approximately **1.8 MB** of source code. This document categorizes all Qt UI framework implementations and IDE components by functional area.

### Key Statistics
- **Total .cpp files**: 118
- **Total .h/.hpp files**: 62
- **Largest file**: MainWindow.cpp (216,393 bytes / ~5,700 lines)
- **Architecture**: Qt6-based with custom hotpatch system
- **Core Pattern**: All components inherit from QObject with Qt signals/slots

---

## 1. CORE QT WIDGET IMPLEMENTATIONS (QMainWindow, QPlainTextEdit, QStatusBar, etc.)

### Primary Application Window

| File | Lines | Type | Description |
|------|-------|------|-------------|
| **MainWindow.cpp** | ~5,700 | Core Widget | **Primary IDE main window** implementing full Qt6 QMainWindow with integrated AI, hotpatch, and development tools. Manages all dock widgets, central editor, menus, and subsystems. Central hub for the entire IDE architecture. |
| **MainWindow.h** | 579 | Core Widget | Header for MainWindow with forward declarations for 60+ subsystem classes. Declares signals/slots for file operations, AI features, hotpatching, and view management. |
| **MainWindow_v5.cpp** | ~1,800 | Core Widget | Streamlined v5 implementation focusing on essential IDE features. Includes initialization phases, menu setup, toolbar creation, and status bar management. |
| **MainWindow_v5.h** | 222 | Core Widget | Clean v5 header with Q_OBJECT macro, slots for file operations, view toggling, and AI operations. |
| **MainWindowMinimal.cpp** | 2,354 bytes | Core Widget | Minimal startup test implementation for rapid iteration and debugging. Contains basic menu, layout, and text editor. |
| **MainWindowMinimal.h** | 250 bytes | Core Widget | Minimal window header - stripped-down for testing purposes. |
| **RawrXDMainWindow.cpp** | 1,763 bytes | Core Widget | Simplified wrapper around MainWindow for specific feature testing. |
| **RawrXDMainWindow.h** | 209 bytes | Core Widget | Minimal window subclass header. |
| **MinimalWindow.cpp** | 714 bytes | Core Widget | Ultra-minimal window for basic Qt testing. |
| **MinimalWindow.h** | 181 bytes | Core Widget | Ultra-minimal window header. |

### Text Editor Components

| File | Lines | Type | Description |
|------|-------|------|-------------|
| **ThemedCodeEditor.cpp** | ~790 | Text Editor | Themeable code editor with syntax highlighting, line numbers, and custom painting. Supports C++, Python, JS, TS, JSON, XML, Markdown. Implements custom text rendering with theme integration. |
| **ThemedCodeEditor.h** | 139 | Text Editor | Header defining `ThemedCodeEditor` (QPlainTextEdit subclass) and `ThemedSyntaxHighlighter` (QSyntaxHighlighter subclass). Includes `LineNumberArea` widget for line number display. |
| **ThemeManager.cpp** | ~1,200 | Text Editor | Manages color schemes, fonts, and theme application across all text editors. Provides light/dark theme switching. |
| **ThemeManager.h** | 158 lines | Text Editor | Theme management interface with color palette definitions and apply methods. |
| **ThemeConfigurationPanel.cpp** | 28,574 bytes | Text Editor | Advanced theme configuration UI with live preview, color picker, font selector. |
| **ThemeConfigurationPanel.h** | 109 lines | Text Editor | Dialog for customizing theme colors, fonts, and editor appearance. |
| **ThemeManager_stub.cpp** | 670 bytes | Text Editor | Stub implementation for testing theme system. |
| **code_minimap.cpp** | 7,193 bytes | Text Editor | Minimap visualization showing bird's-eye view of code. Supports scrolling synchronization with main editor. |
| **code_minimap.h** | 85 lines | Text Editor | Minimap widget header with scroll synchronization interface. |
| **editor_with_minimap.cpp** | 1,316 bytes | Text Editor | Combines code editor with minimap sidebar widget. |
| **editor_with_minimap.h** | 34 lines | Text Editor | Layout container for editor + minimap. |

---

## 2. UI COMPONENTS (Menus, Toolbars, Dialogs, Panels)

### Menu & Toolbar Systems

| File | Lines | Type | Description |
|------|-------|------|-------------|
| **ActivityBar.cpp** | 2,605 bytes | Menu/Toolbar | VS Code-style activity bar (left sidebar icon strip). Manages 7 view buttons (Explorer, Search, Source Control, Debug, Extensions, Settings, Accounts) with hover/active states. |
| **ActivityBar.h** | 91 | Menu/Toolbar | Activity bar widget with ViewType enum, active view tracking, and button management. Emits `viewChanged()` and `viewHovered()` signals. |
| **ActivityBarButton.cpp** | 2,605 bytes | Menu/Toolbar | Individual activity bar button with custom painting, hover effects, and tooltip support. |
| **ActivityBarButton.h** | 50 | Menu/Toolbar | Button component for activity bar with icon management and visual states. |
| **command_palette.hpp** | 71 | Menu/Toolbar | VS Code/Cursor-style command palette (Ctrl+Shift+P) with fuzzy search, categories, and keyboard navigation. |

### Dialog & Panel Widgets

| File | Lines | Type | Description |
|------|-------|------|-------------|
| **settings_dialog.cpp** | 23,338 bytes | Dialog | Comprehensive settings dialog with 7+ tabs: General, Model, AI Chat, Security, Training, CI/CD, Enterprise. Manages application configuration, encryption keys, tokenizers, and CI pipelines. |
| **settings_dialog.h** | 105 | Dialog | Settings dialog class with tab creation methods and UI element declarations. Implements settings save/load/apply flow. |
| **settings_manager.cpp** | 8,056 bytes | Dialog | Persistent settings storage with JSON serialization. Manages user preferences, model configurations, and security settings. |
| **settings_manager.h** | 68 | Dialog | Settings storage interface with key-value access patterns. |
| **ai_chat_panel.cpp** | 50,504 bytes | Dialog/Panel | **Major AI component**: Chat interface with message bubbles, streaming, code highlighting, context awareness. Supports multiple models, chat modes (Max, DeepThinking, ThinkingResearch). |
| **ai_chat_panel.hpp** | 191 | Dialog/Panel | AI chat widget with Message struct, ChatMode enum, context settings, and breadcrumb navigation. |
| **ai_chat_panel_manager.cpp** | 2,617 bytes | Dialog/Panel | Manager for AI chat panel lifecycle, visibility, and model switching. |
| **ai_chat_panel_manager.hpp** | 43 | Dialog/Panel | Chat panel manager interface. |
| **TransparencyControlPanel.cpp** | 15,728 bytes | Dialog/Panel | Advanced transparency/opacity settings panel with real-time preview. |
| **TransparencyControlPanel.h** | 79 | Dialog/Panel | Transparency control UI with value range validation. |
| **ThemeConfigurationPanel.cpp** | 28,574 bytes | Dialog/Panel | Theme editor with color picker, live preview, and persistence. |
| **ThemeConfigurationPanel.h** | 109 | Dialog/Panel | Theme configuration dialog interface. |
| **enterprise_tools_panel.cpp** | 30,772 bytes | Dialog/Panel | Enterprise features panel with compliance logging, security policies, and team management. |
| **enterprise_tools_panel.h** | 133 | Dialog/Panel | Enterprise tools interface with security and compliance controls. |
| **interpretability_panel_enhanced.cpp** | 39,251 bytes | Dialog/Panel | ML interpretability visualization with attention maps, token probabilities, layer activations. |
| **interpretability_panel_enhanced.hpp** | 450 | Dialog/Panel | Enhanced interpretability widget with visualization state management. |
| **interpretability_panel.cpp** | 12,232 bytes | Dialog/Panel | Basic interpretability panel for model analysis. |
| **interpretability_panel.h** | 59 | Dialog/Panel | Interpretability widget header. |

### Terminal & I/O Panels

| File | Lines | Type | Description |
|------|-------|------|-------------|
| **TerminalWidget.cpp** | 3,524 bytes | Panel | Qt terminal emulator wrapper for PowerShell/CMD. Two-phase initialization, process management, output streaming. |
| **TerminalWidget.h** | 40 | Panel | Terminal widget with shell type selection, process lifecycle management. |
| **TerminalManager.cpp** | 2,294 bytes | Panel | Manages terminal process creation, I/O redirection, shell selection. |
| **TerminalManager.h** | 27 | Panel | Terminal manager interface with shell enum and process lifecycle methods. |

---

## 3. MODEL/VIEW LOGIC (File Trees, Output Panels, Tab Systems)

### File Management & Project Explorer

| File | Lines | Type | Description |
|------|-------|------|-------------|
| **file_browser.h** | 37 | Model/View | File browser widget (tree view) for project exploration. Forwards declare dependencies. |
| **file_manager.h** | 166 | Model/View | File I/O operations interface (read, write, delete, rename, directory listing). |

### Multi-Tab & Multi-File Editors

| File | Lines | Type | Description |
|------|-------|------|-------------|
| **multi_tab_editor.h** | 42 | Model/View | Tab widget for multiple open files. Manages editor tabs with open/close/save operations. |
| **multi_file_search.h** | 360 | Model/View | Multi-file search widget with search results list and file navigation. |

### Model Loading & Management

| File | Lines | Type | Description |
|------|-------|------|-------------|
| **model_loader_widget.cpp** | 12,732 bytes | Model/View | UI for GGUF model loading with progress tracking, format detection, and validation. |
| **model_loader_widget.hpp** | 94 | Model/View | Model loader widget with file selection and progress reporting. |
| **model_loader_thread.cpp** | 5,097 bytes | Model/View | Background thread for async model loading to prevent UI freezing. |
| **model_loader_thread.hpp** | 48 | Model/View | Async model loader thread with progress signals. |
| **StreamingGGUFLoader.cpp** | 13,400 bytes | Model/View | Streaming GGUF file loader for memory-efficient model loading. |
| **StreamingGGUFLoader.hpp** | 101 | Model/View | Streaming loader interface for large model files. |
| **model_queue.cpp** | 8,951 bytes | Model/View | Queue management for pending model operations (load, unload, switch). |
| **model_queue.hpp** | 102 | Model/View | Model operation queue with FIFO scheduling. |

### Output & Logging Panels

| File | Lines | Type | Description |
|------|-------|------|-------------|
| **compliance_logger.cpp** | 10,069 bytes | Model/View | Audit logging system for compliance and security tracking. Records all operations with timestamps. |
| **compliance_logger.hpp** | 81 | Model/View | Compliance logger interface with level-based filtering. |
| **metrics_collector.cpp** | 9,060 bytes | Model/View | System metrics collection (CPU, memory, GPU usage) with history. |
| **metrics_collector.hpp** | 110 | Model/View | Metrics interface with time-series data collection. |
| **health_check_server.cpp** | 19,283 bytes | Model/View | HTTP health check endpoint for system status monitoring. |
| **health_check_server.hpp** | 94 | Model/View | Health check server interface. |

### Chat & Message UI

| File | Lines | Type | Description |
|------|-------|------|-------------|
| **agent_chat_breadcrumb.cpp** | 35,052 bytes | Model/View | Chat breadcrumb navigation showing conversation context and selected code snippets. |
| **agent_chat_breadcrumb.hpp** | 173 | Model/View | Breadcrumb widget for chat context display. |
| **chat_session.cpp** | 2,405 bytes | Model/View | Manages single chat session state (messages, context, model info). |
| **chat_session.hpp** | 70 | Model/View | Chat session data structure with message history. |

---

## 4. EVENT HANDLING & SIGNAL/SLOT MECHANISMS

### Agent Mode Handlers

| File | Lines | Type | Description |
|------|-------|------|-------------|
| **agent_mode_handler.cpp** | 7,467 bytes | Event Handler | Base agent mode handler for routing commands to appropriate agent (Plan, Agent, Ask, Architect). |
| **agent_mode_handler.hpp** | 180 | Event Handler | Agent mode handler interface with mode enum (PLAN, AGENT, ASK, ARCHITECT). |
| **plan_mode_handler.cpp** | 9,107 bytes | Event Handler | Plan mode handler executing planning workflows. |
| **plan_mode_handler.hpp** | 165 | Event Handler | Plan mode interface for multi-step planning. |
| **ask_mode_handler.cpp** | 6,707 bytes | Event Handler | Ask mode handler for Q&A interactions. |
| **ask_mode_handler.hpp** | 104 | Event Handler | Ask mode interface for question answering. |

### Hotpatch System (Event-Driven)

| File | Lines | Type | Description |
|------|-------|------|-------------|
| **unified_hotpatch_manager.cpp** | 19,357 bytes | Event Handler | **Critical**: Coordinates three-layer hotpatching (memory, byte-level, server). Emits signals: `patchApplied()`, `errorOccurred()`, `optimizationComplete()`. Manages unified statistics and preset save/load. |
| **unified_hotpatch_manager.hpp** | 147 | Event Handler | Hotpatch manager interface with slot declarations for memory, byte, and server patches. |
| **model_memory_hotpatch.cpp** | 29,702 bytes | Event Handler | Direct RAM patching using OS memory protection (VirtualProtect/mprotect). Cross-platform abstractions. |
| **model_memory_hotpatch.hpp** | 204 | Event Handler | Memory hotpatcher with PatchResult struct and OS-level memory modification. |
| **byte_level_hotpatcher.cpp** | 14,755 bytes | Event Handler | GGUF binary file manipulation with Boyer-Moore pattern matching. Zero-copy directWrite/directRead operations. |
| **byte_level_hotpatcher.hpp** | 134 | Event Handler | Byte-level hotpatcher interface with atomic operations (swap, XOR, rotate). |
| **gguf_server_hotpatch.cpp** | 21,707 bytes | Event Handler | Server-layer hotpatching for inference request/response transformation. Injection points: PreRequest, PostRequest, PreResponse, PostResponse, StreamChunk. |
| **gguf_server_hotpatch.hpp** | 217 | Event Handler | Server hotpatch struct with transform functions and caching. |
| **proxy_hotpatcher.cpp** | 24,121 bytes | Event Handler | Agentic proxy-layer byte manipulation for output correction. Token logit bias support (RST injection). |
| **proxy_hotpatcher.hpp** | 221 | Event Handler | Proxy hotpatcher with `void*` custom validator (avoids MSVC template issues). |

### Agentic Failure Recovery

| File | Lines | Type | Description |
|------|-------|------|-------------|
| **agentic_failure_detector.cpp** | 18,568 bytes | Event Handler | Pattern-based failure detection: refusal, hallucination, timeout, resource exhaustion, safety violations. Confidence scoring 0.0-1.0. Emits Qt signals asynchronously. |
| **agentic_failure_detector.hpp** | Not listed | Event Handler | Failure detector with FailureType enum and confidence scoring. |
| **agentic_puppeteer.cpp** | 19,649 bytes | Event Handler | Automatic response correction for detected failures. Mode-specific formatting (Plan, Agent, Ask). Uses static factory `CorrectionResult::ok()/error()`. |
| **agentic_self_corrector.cpp** | 10,248 bytes | Event Handler | Self-correction logic for agentic responses. |
| **agentic_self_corrector.hpp** | 83 | Event Handler | Self-corrector interface. |

### AI Integration & Streaming

| File | Lines | Type | Description |
|------|-------|------|-------------|
| **ai_code_assistant.cpp** | 9,274 bytes | Event Handler | AI code assistant with real-time suggestions, explanations, and refactoring. |
| **ai_code_assistant.h** | 154 | Event Handler | Code assistant interface. |
| **ai_code_assistant_panel.cpp** | 11,051 bytes | Event Handler | UI panel for AI assistant with suggestion display. |
| **ai_code_assistant_panel.h** | 77 | Event Handler | Assistant panel widget. |
| **ai_code_assistant_real.cpp** | 15,706 bytes | Event Handler | Production implementation of AI assistant. |
| **ai_code_assistant_panel_real.cpp** | 13,476 bytes | Event Handler | Production assistant panel. |
| **ai_completion_provider.cpp** | 6,866 bytes | Event Handler | Code completion suggestions from AI model. |
| **ai_completion_provider.h** | 102 | Event Handler | Completion provider interface. |
| **ai_switcher.cpp** | 1,962 bytes | Event Handler | UI for switching between different AI providers/models. |
| **ai_switcher.hpp** | 26 | Event Handler | Model switcher widget. |
| **streaming_inference.cpp** | 1,553 bytes | Event Handler | Streaming inference output handling. |
| **streaming_inference.hpp** | 22 | Event Handler | Streaming inference interface. |
| **streaming_inference_api.cpp** | 3,874 bytes | Event Handler | HTTP API for streaming model inference. |
| **streaming_inference_api.hpp** | 94 | Event Handler | Streaming API interface. |

### Model & Inference Engines

| File | Lines | Type | Description |
|------|-------|------|-------------|
| **inference_engine.cpp** | 68,268 bytes | Event Handler | **Large component**: Core inference execution with model loading, prompt processing, token generation. Supports streaming, batching, and multiple backends. |
| **inference_engine.hpp** | 438 | Event Handler | Inference engine interface with configuration and execution methods. |
| **transformer_inference.cpp** | 25,339 bytes | Event Handler | Transformer-specific inference logic with attention caching. |
| **transformer_inference.hpp** | 162 | Event Handler | Transformer inference interface. |
| **gguf_server.cpp** | 24,977 bytes | Event Handler | GGUF model server with HTTP endpoints for inference. |
| **gguf_server.hpp** | 155 | Event Handler | GGUF server interface. |
| **gguf_loader.cpp** | 8,370 bytes | Event Handler | GGUF file format parser and model loader. |
| **gguf_loader.h** | 226 | Event Handler | GGUF loader interface. |
| **gguf_loader_stub.cpp** | 2,609 bytes | Event Handler | Stub loader for testing. |
| **gguf_loader_stub.hpp** | 23 | Event Handler | Stub loader interface. |

### Model Monitoring & Management

| File | Lines | Type | Description |
|------|-------|------|-------------|
| **model_monitor.cpp** | 2,949 bytes | Event Handler | Real-time model performance monitoring (latency, throughput, resource usage). |
| **model_monitor.hpp** | 27 | Event Handler | Monitor interface. |
| **model_loader_with_compression.cpp** | 10,907 bytes | Event Handler | GGUF loader with compression support (brutal_gzip). |
| **model_loader_with_compression.hpp** | 77 | Event Handler | Compressed loader interface. |

---

## 5. UTILITY FUNCTIONS & HELPERS

### Tokenizers & Encoding

| File | Lines | Type | Description |
|------|-------|------|-------------|
| **bpe_tokenizer.cpp** | 11,253 bytes | Utility | BPE (Byte-Pair Encoding) tokenizer implementation for model input encoding. |
| **bpe_tokenizer.hpp** | 125 | Utility | BPE tokenizer interface. |
| **sentencepiece_tokenizer.cpp** | 12,963 bytes | Utility | SentencePiece tokenizer implementation. |
| **sentencepiece_tokenizer.hpp** | 107 | Utility | SentencePiece interface. |
| **tokenizer_selector.cpp** | 17,010 bytes | Utility | UI for selecting and configuring tokenizers. |
| **tokenizer_selector.h** | 8 | Utility | Tokenizer selector widget. |
| **tokenizer_language_selector.cpp** | 5,587 bytes | Utility | Language-specific tokenizer selection. |
| **tokenizer_language_selector.h** | 60 | Utility | Language selector interface. |
| **vocabulary_loader.cpp** | 19,864 bytes | Utility | Load and manage vocabulary files. |
| **vocabulary_loader.hpp** | 109 | Utility | Vocabulary loader interface. |
| **codec.cpp** | 1,322 bytes | Utility | Text encoding/decoding utilities. |
| **codec.h** | 9 | Utility | Codec interface. |

### Compression & Data Handling

| File | Lines | Type | Description |
|------|-------|------|-------------|
| **brutal_gzip_wrapper.cpp** | 1,778 bytes | Utility | Qt wrapper for brutal-gzip compression library. |
| **brutal_gzip.h** | 18 | Utility | Brutal gzip header. |
| **deflate_brutal_qt.hpp** | 115 | Utility | Deflate/inflate glue for Qt integration. |
| **inflation_deflate_cpp.cpp** | 1,367 bytes | Utility | Deflate algorithm implementation. |
| **compression_interface.h** | 184 | Utility | Abstract compression interface. |
| **compression_wrappers.h** | 61 | Utility | Compression wrapper utilities. |

### Backend & Hardware Selection

| File | Lines | Type | Description |
|------|-------|------|-------------|
| **gpu_backend.cpp** | 12,470 bytes | Utility | GPU backend selection (CUDA, ROCm, Metal, Vulkan). |
| **gpu_backend.hpp** | 116 | Utility | GPU backend interface. |
| **hardware_backend_selector.h** | 148 | Utility | UI for selecting compute backend. |
| **gpu_inference_benchmark.cpp** | 9,805 bytes | Utility | GPU performance benchmarking. |
| **simple_gpu_test.cpp** | 5,859 bytes | Utility | Basic GPU functionality test. |
| **unified_backend.cpp** | 8,358 bytes | Utility | Unified interface for multiple backends. |
| **unified_backend.hpp** | 75 | Utility | Backend abstraction layer. |
| **vulkan_compute.h** | 230 | Utility | Vulkan compute shader interface. |

### Quantization & Optimization

| File | Lines | Type | Description |
|------|-------|------|-------------|
| **quant_utils.cpp** | 7,550 bytes | Utility | Quantization utilities (int8, int4, fp8 quantization schemes). |
| **quant_utils.hpp** | 23 | Utility | Quantization interface. |
| **quant_impl.cpp** | 4,400 bytes | Utility | Quantization implementation details. |
| **layer_quant_widget.cpp** | 3,858 bytes | Utility | UI for per-layer quantization control. |
| **layer_quant_widget.hpp** | 39 | Utility | Quantization control widget. |
| **transformer_block_scalar.h** | 77 | Utility | Scalar transformer block optimization. |

### MASM Integration & Feature Management

| File | Lines | Type | Description |
|------|-------|------|-------------|
| **masm_feature_manager.cpp** | 39,314 bytes | Utility | Manages MASM-accelerated features (hotpatching, inference kernels). |
| **masm_feature_manager.hpp** | 266 | Utility | MASM feature manager interface. |
| **masm_feature_settings_panel.cpp** | 20,527 bytes | Utility | Settings panel for MASM feature tuning. |
| **masm_feature_settings_panel.hpp** | 59 | Utility | MASM settings panel widget. |
| **masm_hotpatch_bridge.cpp** | 3,777 bytes | Utility | Bridge between Qt hotpatch system and MASM assembly code. |
| **masm_orchestration_stubs.cpp** | 1,393 bytes | Utility | Stub implementations for MASM orchestration. |

### Training & Checkpointing

| File | Lines | Type | Description |
|------|-------|------|-------------|
| **checkpoint_manager.cpp** | 15,239 bytes | Utility | Model checkpoint save/load with versioning. |
| **checkpoint_manager.h** | 5 | Utility | Checkpoint manager interface. |
| **advanced_checkpoint_manager.cpp** | 6,567 bytes | Utility | Advanced checkpointing with snapshots and rollback. |
| **advanced_checkpoint_manager.h** | 46 | Utility | Advanced checkpoint interface. |
| **distributed_trainer.cpp** | 23,145 bytes | Utility | Distributed training orchestration. |
| **distributed_trainer.h** | 59 | Utility | Distributed trainer interface. |
| **backup_manager.cpp** | 13,861 bytes | Utility | Backup and recovery management. |
| **backup_manager.hpp** | 84 | Utility | Backup manager interface. |

### Security & Compliance

| File | Lines | Type | Description |
|------|-------|------|-------------|
| **security_manager.cpp** | 17,945 bytes | Utility | Encryption, API key management, and security policies. |
| **security_manager.h** | 318 | Utility | Security manager interface with encryption methods. |
| **sla_manager.cpp** | 9,476 bytes | Utility | SLA tracking and reporting for compliance. |
| **sla_manager.hpp** | 111 | Utility | SLA manager interface. |

### Utilities & Helpers

| File | Lines | Type | Description |
|------|-------|------|-------------|
| **PathResolver.h** | 186 | Utility | Path resolution for files, resources, and configuration. |
| **todo_manager.cpp** | 3,901 bytes | Utility | TODO comment tracking and management. |
| **todo_manager.h** | 30 | Utility | TODO manager interface. |
| **debug_logger.h** | 22 | Utility | Debug logging macros and functions. |
| **settings.h** | 51 | Utility | Settings storage constants. |
| **Subsystems.h** | 117 | Utility | Forward declarations for all IDE subsystems. |

### Production & Integration Tests

| File | Lines | Type | Description |
|------|-------|------|-------------|
| **production_integration_test.cpp** | 14,649 bytes | Utility | Integration tests for all major subsystems. |
| **production_integration_example.cpp** | 9,639 bytes | Utility | Example integration usage. |
| **production_feature_test.cpp** | 13,288 bytes | Utility | Feature-specific testing. |
| **gguf_hotpatch_tester.cpp** | 7,792 bytes | Utility | Hotpatch system testing. |

### Proxies & Adapters

| File | Lines | Type | Description |
|------|-------|------|-------------|
| **ollama_hotpatch_proxy.cpp** | 13,809 bytes | Utility | Proxy for Ollama API with hotpatch injection. |
| **ollama_hotpatch_proxy.hpp** | 191 | Utility | Ollama proxy interface. |
| **ollama_proxy.h** | 60 | Utility | Basic Ollama proxy. |

### Agentic Tools & Execution

| File | Lines | Type | Description |
|------|-------|------|-------------|
| **agentic_tools.cpp** | 11,893 bytes | Utility | Tools available to agentic execution (file ops, code analysis, etc.). |
| **agentic_tools.hpp** | 78 | Utility | Tool definitions for agents. |
| **agentic_textedit.cpp** | 5,861 bytes | Utility | Text editor component for agentic modifications. |
| **agentic_textedit.hpp** | 61 | Utility | Agentic text editor. |
| **agentic_text_edit.h** | 120 | Utility | Alternative agentic editor. |

### Miscellaneous

| File | Lines | Type | Description |
|------|-------|------|-------------|
| **ComponentFactory.cpp** | 11,934 bytes | Utility | Factory for creating UI components dynamically. |
| **ComponentFactory.h** | 93 | Utility | Component factory interface. |
| **test_chat_console.cpp** | 12,597 bytes | Utility | Test harness for chat functionality. |
| **test_chat_streaming.cpp** | 4,233 bytes | Utility | Streaming chat test. |
| **experimental_features_menu.cpp** | 6,684 bytes | Utility | Menu for toggling experimental runtime features. |
| **experimental_features_menu.hpp** | 86 | Utility | Experimental features interface. |
| **ci_cd_settings.cpp** | 14,299 bytes | Utility | CI/CD pipeline configuration. |
| **ci_pipeline_manager.cpp** | 6,690 bytes | Utility | CI pipeline orchestration. |
| **ci_pipeline_manager.h** | 54 | Utility | Pipeline manager interface. |
| **model_benchmark_console.cpp** | 7,154 bytes | Utility | Performance benchmarking console. |
| **agentic_copilot_bridge.h** | 145 | Utility | Bridge to GitHub Copilot-like functionality. |

---

## 6. ENTRY POINTS & APPLICATION INITIALIZATION

| File | Lines | Type | Description |
|------|-------|------|-------------|
| **main_qt.cpp** | 3,403 bytes | Init | Qt application entry point. Initializes QApplication, creates MainWindow, applies initial theme. |
| **main_v5.cpp** | 2,996 bytes | Init | Version 5 entry point with streamlined initialization. |
| **minimal_ide_main.cpp** | 2,053 bytes | Init | Minimal IDE launcher for testing. |
| **minimal_main.cpp** | 869 bytes | Init | Ultra-minimal main for basic testing. |
| **minimal_qt_test.cpp** | 5,777 bytes | Init | Qt functionality test harness. |
| **test_qt.cpp** | 291 bytes | Init | Basic Qt test. |
| **test_enum.cpp** | 115 bytes | Init | Enum testing. |

---

## 7. ADVANCED UI FEATURES

### Breadcrumb Navigation & Context

| File | Lines | Type | Description |
|------|-------|------|-------------|
| **agent_chat_breadcrumb.cpp** | 35,052 bytes | Advanced UI | Chat breadcrumb showing conversation flow, selected code context, and previous interactions. |
| **agent_chat_breadcrumb.hpp** | 173 | Advanced UI | Breadcrumb widget for navigation history. |

### Telemetry & Observability

| File | Lines | Type | Description |
|------|-------|------|-------------|
| **EnterpriseTelemetry.h** | 48 | Advanced UI | Enterprise telemetry interface. |
| **TelemetryWindow.h** | 87 | Advanced UI | Telemetry display window. |

---

## ARCHITECTURE PATTERNS & KEY INSIGHTS

### Pattern 1: Three-Layer Hotpatching Coordination
All hotpatch components follow a unified pattern through `UnifiedHotpatchManager`:
```cpp
PatchResult applyMemoryPatch(...)      // Memory layer
UnifiedResult applyBytePatch(...)      // Byte layer  
void addServerHotpatch(...)            // Server layer
```

### Pattern 2: Qt Signal/Slot for Async Operations
All async operations use Qt signals instead of callbacks:
```cpp
connect(inference, &InferenceEngine::streamingOutput, 
        chatPanel, &AIChatPanel::updateStreamingMessage);
```

### Pattern 3: QObject Ownership & Memory Management
All widgets follow Qt's RAII pattern with parent ownership:
```cpp
QMainWindow // owns all dock widgets via addDockWidget()
m_chatPanel = new AIChatPanel(this);  // Qt deletes when MainWindow destroyed
```

### Pattern 4: Two-Phase Initialization
Many components use two-phase initialization:
```cpp
// Phase 1: Construction (safe for any context)
AIChatPanel* panel = new AIChatPanel();

// Phase 2: Initialize (requires QApplication ready)
panel->initialize();  // Creates widgets, applies theme, connects signals
```

### Pattern 5: Factory Methods for Results
No exception throwing; all operations return result objects:
```cpp
// Memory hotpatcher
PatchResult result = memory->applyPatch(patch);
if (!result.success) { /* handle via errorOccurred() signal */ }

// Server hotpatcher  
ServerHotpatch hotpatch = createServerHotpatch(...);
if (!hotpatch.isValid) { /* error handling */ }
```

---

## FILE SIZE DISTRIBUTION

| Category | Total Size | Avg File Size | Count |
|----------|-----------|---------------|-------|
| Core Qt Widgets | ~535 KB | ~61 KB | 9 |
| UI Components | ~425 KB | ~23 KB | 18 |
| Model/View Logic | ~245 KB | ~18 KB | 13 |
| Event Handlers | ~580 KB | ~28 KB | 21 |
| Utilities | ~425 KB | ~12 KB | 35 |
| Initialization | ~17 KB | ~2.4 KB | 7 |
| **TOTAL** | **~2.2 MB** | **~12 KB** | **103** |

---

## CRITICAL COMPONENTS FOR IDE FUNCTIONALITY

### Tier 1: Essential
1. **MainWindow.cpp/h** - Central IDE window
2. **ThemedCodeEditor.cpp/h** - Main text editor
3. **TerminalWidget.cpp/h** - Terminal integration
4. **AIChatPanel.cpp/hpp** - AI assistance core
5. **inference_engine.cpp/hpp** - Model inference

### Tier 2: Important
6. **unified_hotpatch_manager.cpp/hpp** - Three-layer hotpatching
7. **settings_dialog.cpp/h** - Configuration
8. **ai_chat_panel.cpp/hpp** - Chat UI
9. **ActivityBar.cpp/h** - Navigation
10. **command_palette.hpp** - Quick actions

### Tier 3: Enhancement
11-20: Model loaders, monitoring, specialized panels, and utilities

---

## BUILD INTEGRATION

All files compile via CMake with Qt MOC (Meta-Object Compiler) enabled:

```cmake
set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 20)

# Qt6 discovery
find_package(Qt6 COMPONENTS Core Gui Widgets WebEngineWidgets REQUIRED)

# All .h files with Q_OBJECT macro auto-generate moc_*.cpp
# All signal/slot connections must match MOC-generated metadata
```

### Key Build Considerations
- **Qt MOC**: Automatically processes all Q_OBJECT classes
- **Include guards**: All .h/.hpp files protected against circular includes
- **Forward declarations**: Extensive use to reduce compile times
- **Signal/slot validation**: Checked at link time via MOC
- **Const correctness**: Const methods must be const; non-const method calls require copies (QByteArray)

---

## USAGE RECOMMENDATIONS

### For Adding New Panels
1. Create new widget class inheriting from `QWidget`
2. Add Q_OBJECT macro and declare slots with `private slots:`
3. Implement two-phase init: constructor + initialize() slot
4. Register with MainWindow via `addDockWidget(Qt::RightDockWidgetArea, ...)`
5. Add toggle slot to MainWindow's View menu

### For Adding New AI Features
1. Extend `AIChatPanel` or create new panel inheriting from `QWidget`
2. Connect to `InferenceEngine::streamingOutput` signal
3. Use `agentic_puppeteer.cpp` for response correction if needed
4. Register commands in `CommandPalette` for discoverability

### For Hotpatching Support
1. Use `UnifiedHotpatchManager::applyMemoryPatch()` for runtime changes
2. Use `UnifiedHotpatchManager::applyBytePatch()` for binary modifications
3. Use `UnifiedHotpatchManager::addServerHotpatch()` for proxy layer
4. All return `PatchResult`/`UnifiedResult` structs; emit signals on completion

---

## RECENT CHANGES (Build Status: December 28, 2025)

✅ **All systems integrated**:
- Core Qt widgets fully functional
- Three-layer hotpatching operational
- Agentic failure detection & correction
- AI chat with streaming support
- Terminal integration
- Theme/settings management
- Model loading and inference
- Compliance & security logging

**Known Limitations**:
- Some enterprise features stub implementations (CI/CD panel)
- Telemetry window minimal implementation
- Some theme customization options incomplete

---

## REFERENCES & RELATED DOCUMENTATION

- **BUILD_COMPLETE.md** - Latest build status and fixes
- **COPILOT_INSTRUCTIONS.md** - Architecture overview and patterns
- **CMakeLists.txt** - Build configuration (891 lines)
- **Architecture.md** - Detailed system design
- **Qt6 Documentation** - Official Qt6 API reference

---

**End of Document**  
For questions or updates, refer to the RawrXD team documentation in the project repository.
