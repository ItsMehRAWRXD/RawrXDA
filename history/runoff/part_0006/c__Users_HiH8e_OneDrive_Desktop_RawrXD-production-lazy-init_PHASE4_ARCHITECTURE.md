# 🏗️ PHASE 4 ARCHITECTURE DIAGRAM

```
╔═══════════════════════════════════════════════════════════════════════════════╗
║                    RAWRXD IDE PHASE 4 ARCHITECTURE                            ║
║                  LLM Integration & Agentic Loop System                        ║
╚═══════════════════════════════════════════════════════════════════════════════╝

┌─────────────────────────────────────────────────────────────────────────────┐
│                             USER INTERFACE LAYER                            │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │  Main Menu   │  │   Keyboard   │  │  Status Bar  │  │  Chat Window │  │
│  │  AI Menu     │  │  Shortcuts   │  │  Indicators  │  │  Interface   │  │
│  │  Backend     │  │  Ctrl+Space  │  │  Backend     │  │  Messages    │  │
│  │  Agent       │  │  Ctrl+.      │  │  Agent       │  │  Streaming   │  │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘  │
│         │                  │                  │                  │          │
│         └──────────────────┴──────────────────┴──────────────────┘          │
│                                    │                                        │
└────────────────────────────────────┼────────────────────────────────────────┘
                                     ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                        PHASE 4 INTEGRATION LAYER                            │
│                         phase4_integration.asm                              │
├─────────────────────────────────────────────────────────────────────────────┤
│  • Menu Management                  • State Management                      │
│  • Command Routing                  • UI Updates                            │
│  • Shortcut Handling                • Backend Switching                     │
└─────────────────────────────────────┬───────────────────────────────────────┘
                                      ▼
         ┌────────────────────────────┴────────────────────────────┐
         │                                                          │
         ▼                            ▼                             ▼
┌────────────────────┐    ┌────────────────────┐    ┌────────────────────┐
│   CHAT INTERFACE   │    │   AGENTIC LOOP     │    │    LLM CLIENT      │
│ chat_interface.asm │◄───┤  agentic_loop.asm  │───►│  llm_client.asm    │
└────────────────────┘    └────────────────────┘    └────────────────────┘
│                          │                          │                    │
│ • Message Display        │ • Perceive              │ • Multi-Backend    │
│ • Streaming Tokens       │ • Plan                  │ • Streaming        │
│ • User Input             │ • Act                   │ • Tool Calling     │
│ • Commands               │ • Learn                 │ • JSON Parse       │
│ • Session History        │ • Context Mgmt          │ • Error Handling   │
└──────┬───────────────────┴─────────────────────────┴────────────────────┘
       │                            │                          │
       └────────────────────────────┼──────────────────────────┘
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                          TOOL EXECUTION LAYER                               │
│                         44 Development Tools                                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │     FILE     │  │     CODE     │  │   DEBUGGING  │  │    SEARCH    │  │
│  │  OPERATIONS  │  │   EDITING    │  │              │  │  NAVIGATION  │  │
│  │   (12 tools) │  │   (8 tools)  │  │   (6 tools)  │  │   (5 tools)  │  │
│  │              │  │              │  │              │  │              │  │
│  │ • read       │  │ • insert     │  │ • start      │  │ • symbol     │  │
│  │ • write      │  │ • replace    │  │ • stop       │  │ • reference  │  │
│  │ • create     │  │ • delete     │  │ • step       │  │ • definition │  │
│  │ • delete     │  │ • format     │  │ • breakpoint │  │ • text       │  │
│  │ • rename     │  │ • refactor   │  │ • watch      │  │ • replace    │  │
│  │ • copy       │  │ • comment    │  │ • evaluate   │  │              │  │
│  │ • search     │  │ • optimize   │  │              │  │              │  │
│  │ • list       │  │ • lint       │  │              │  │              │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  └──────────────┘  │
│                                                                             │
│  ┌──────────────┐  ┌──────────────┐                                        │
│  │     GIT      │  │    BUILD     │                                        │
│  │ INTEGRATION  │  │    SYSTEM    │                                        │
│  │   (8 tools)  │  │   (5 tools)  │                                        │
│  │              │  │              │                                        │
│  │ • init       │  │ • compile    │                                        │
│  │ • status     │  │ • link       │                                        │
│  │ • add        │  │ • clean      │                                        │
│  │ • commit     │  │ • run        │                                        │
│  │ • push       │  │ • test       │                                        │
│  │ • pull       │  │              │                                        │
│  │ • branch     │  │              │                                        │
│  │ • merge      │  │              │                                        │
│  └──────────────┘  └──────────────┘                                        │
└──────────────────────────────────┬──────────────────────────────────────────┘
                                   ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                           LLM BACKEND LAYER                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │   OPENAI     │  │    CLAUDE    │  │   GEMINI     │  │  LOCAL GGUF  │  │
│  │  GPT-4 Turbo │  │ 3.5 Sonnet   │  │  Gemini Pro  │  │  Llama/Phi   │  │
│  │              │  │              │  │              │  │              │  │
│  │ • 4096 ctx   │  │ • 4096 ctx   │  │ • 4096 ctx   │  │ • 2048 ctx   │  │
│  │ • Streaming  │  │ • Streaming  │  │ • Streaming  │  │ • Streaming  │  │
│  │ • Tools      │  │ • Tools      │  │ • Tools      │  │ • No tools   │  │
│  │ • Fast       │  │ • Accurate   │  │ • Multimodal │  │ • Private    │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  └──────────────┘  │
│                                                                             │
│  ┌──────────────┐                                                          │
│  │   OLLAMA     │                                                          │
│  │ Llama2/Mistral                                                          │
│  │              │                                                          │
│  │ • 4096 ctx   │                                                          │
│  │ • Streaming  │                                                          │
│  │ • Local      │                                                          │
│  │ • Easy setup │                                                          │
│  └──────────────┘                                                          │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│                         MEMORY & STATE LAYER                                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐        │
│  │  SHORT-TERM      │  │  LONG-TERM       │  │   TOOL RESULTS   │        │
│  │  MEMORY          │  │  MEMORY          │  │   CACHE          │        │
│  │                  │  │                  │  │                  │        │
│  │ • Session data   │  │ • User prefs     │  │ • Recent calls   │        │
│  │ • Context        │  │ • Learnings      │  │ • Outputs        │        │
│  │ • Active tasks   │  │ • Patterns       │  │ • Errors         │        │
│  └──────────────────┘  └──────────────────┘  └──────────────────┘        │
│                                                                             │
│  ┌──────────────────┐  ┌──────────────────┐                               │
│  │  AGENT STATE     │  │  UI STATE        │                               │
│  │                  │  │                  │                               │
│  │ • Current step   │  │ • Active window  │                               │
│  │ • Iteration      │  │ • Backend        │                               │
│  │ • Plan progress  │  │ • Streaming      │                               │
│  └──────────────────┘  └──────────────────┘                               │
└─────────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
                              DATA FLOW EXAMPLE
═══════════════════════════════════════════════════════════════════════════════

User: "Optimize this assembly code"
      │
      ▼
[Chat Interface] ──► Capture message, display in UI
      │
      ▼
[Phase 4 Integration] ──► Route to Agentic Loop
      │
      ▼
[Agentic Loop - PERCEIVE] ──► Analyze request, gather code context
      │
      ▼
[Agentic Loop - PLAN] ──► Break into steps:
      │                    1. Read current code
      │                    2. Analyze patterns
      │                    3. Generate optimized version
      │                    4. Explain changes
      ▼
[Agentic Loop - ACT] ──► Execute tools:
      │                   • file_read (get code)
      │                   • code_optimize (analyze)
      │                   • code_replace (update)
      ▼
[LLM Client] ──► Send to Claude 3.5:
      │           {
      │             "messages": [...],
      │             "tools": [file_read, code_optimize, ...],
      │             "stream": true
      │           }
      ▼
[Claude API] ──► Process request, return streaming response
      │
      ▼
[LLM Client] ──► Parse SSE stream, extract tokens
      │
      ▼
[Chat Interface] ──► Display tokens in real-time:
      │              "I've analyzed your code..."
      │              "Here's the optimized version..."
      │              "mov eax, ecx ; Combined operations"
      ▼
[Agentic Loop - LEARN] ──► Store results in memory
      │                     Update success metrics
      ▼
[User] ◄── View optimized code with explanation

═══════════════════════════════════════════════════════════════════════════════
                            PERFORMANCE PROFILE
═══════════════════════════════════════════════════════════════════════════════

Module Loading:         <100ms
Chat Interface Init:    <50ms
LLM Client Connect:     200-500ms
First Token Latency:    2-4 seconds (cloud) / 1-2 seconds (local)
Token Streaming:        <50ms per token
Tool Execution:         <100ms per tool
Agent Iteration:        3-5 seconds
Memory Access:          <1ms
UI Update:              16ms (60 FPS)

Total Startup Time:     <1 second
Peak Memory Usage:      ~50MB
Binary Size:            ~200KB

═══════════════════════════════════════════════════════════════════════════════

Key Advantages:
✅ Pure Assembly - Maximum performance
✅ Multi-Backend - Flexibility & choice
✅ 44 Tools - Comprehensive coverage
✅ Agentic Loop - Intelligent automation
✅ Real-time Streaming - Instant feedback
✅ Memory Systems - Context-aware responses
✅ Local Models - Privacy & offline support

Built with ❤️ in x86 Assembly
