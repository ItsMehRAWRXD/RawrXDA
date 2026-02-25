; RawrXD_Unified_Manifest.asm - Consolidated extension replacement
; Replaces: your-name.cursor-simple-ai, rawrz-underground.rawrz-agentic, ItsMehRAWRXD.rawrxd-lsp-client,
;           undefined_publisher.cursor-ollama-proxy, your-name.cursor-multi-ai, bigdaddyg.bigdaddyg-copilot,
;           undefined_publisher.bigdaddyg-cursor-chat, bigdaddyg.bigdaddyg-asm-ide, undefined_publisher.bigdaddyg-asm-extension
; With: Zero-dependency MASM64 modules

EXTENSION_MANIFEST SEGMENT READONLY ALIGN(64)

; Extension 1: Simple AI -> ModelPipeline
db "cursor-simple-ai", 0
dd OFFSET OllamaGenerate
dd OFFSET TensorDequantQ4_0_AVX512

; Extension 2: Agentic -> AgentCore
db "rawrz-agentic", 0
dd OFFSET AgentOrchestrate
dd OFFSET AutonomousLoop

; Extension 3: LSP Client -> LSPBridge
db "rawrxd-lsp-client", 0
dd OFFSET LSP_Initialize
dd OFFSET LSP_CodeComplete

; Extension 4: Ollama Proxy -> Native HTTP
db "cursor-ollama-proxy", 0
dd OFFSET WinHttpCallback
dd OFFSET ProcessStreamChunk

; Extension 5: Multi-AI -> ModelRouter
db "cursor-multi-ai", 0
dd OFFSET RouteToClaude
dd OFFSET RouteToGPT
dd OFFSET RouteToLocal

; Extension 6: Copilot -> CompletionEngine
db "bigdaddyg-copilot", 0
dd OFFSET InlineComplete
dd OFFSET GhostTextShow

; Extension 7: Cursor Chat -> Sidebar
db "bigdaddyg-cursor-chat", 0
dd OFFSET ChatAppendToken
dd OFFSET ChatStreamBegin

; Extension 8: ASM IDE -> Genesis
db "bigdaddyg-asm-ide", 0
dd OFFSET GenesisMain
dd OFFSET AssembleFile

; Extension 9: ASM Extension -> Installer
db "bigdaddyg-asm-extension", 0
dd OFFSET InstallExtension
dd OFFSET RegisterLanguage

EXTENSION_MANIFEST ENDS
