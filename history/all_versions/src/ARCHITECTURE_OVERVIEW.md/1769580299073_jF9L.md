# RawrXD Compiler Engine - Architecture Overview

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                      COMPILER ENGINE                                 │
│                    (MASM64 Implementation)                           │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                      PUBLIC API LAYER                                │
├─────────────────────────────────────────────────────────────────────┤
│                                                                       │
│  CompilerEngine_Create()    - Create engine instance                │
│  CompilerEngine_Destroy()   - Destroy engine & free resources       │
│  CompilerEngine_Compile()   - Compile source code                   │
│  CompilerEngine_ValidateOptions() - Validate compile options        │
│                                                                       │
└─────────────────────────────────────────────────────────────────────┘
                                   ↓
┌─────────────────────────────────────────────────────────────────────┐
│                    COMPILATION PIPELINE                              │
├──────────────────┬──────────────────┬──────────────────┬─────────────┤
│                  │                  │                  │             │
│  Stage 1: LEXING │  Stage 2: PARSE  │  Stage 3: SEM   │  Stage 4:IR │
│  Tokenization    │  AST Building    │  Type Check     │  Code Gen   │
│                  │                  │                  │             │
└──────────────────┴──────────────────┴──────────────────┴─────────────┘
                                   ↓
┌──────────────────┬──────────────────┬──────────────────┬─────────────┐
│                  │                  │                  │             │
│ Stage 5: OPTIM   │ Stage 6: CODEGEN │ Stage 7: ASSEM  │ Stage 8:LNK │
│ Const Fold       │ Target-Specific  │ Object Gen      │ Exe Create  │
│ DCE              │ Codegen          │                 │             │
│                  │                  │                  │             │
└──────────────────┴──────────────────┴──────────────────┴─────────────┘
                                   ↓
┌─────────────────────────────────────────────────────────────────────┐
│                       OUTPUT (Binary)                                 │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                  WORKER THREAD POOL LAYER                            │
├─────────────────────────────────────────────────────────────────────┤
│                                                                       │
│  ┌────────────────┐  ┌────────────────┐  ┌────────────────┐         │
│  │ Worker Thread 1│  │ Worker Thread 2│  │ Worker Thread 3│ ...     │
│  │ ┌────────────┐ │  │ ┌────────────┐ │  │ ┌────────────┐ │         │
│  │ │ Options    │ │  │ │ Options    │ │  │ │ Options    │ │         │
│  │ │ Result     │ │  │ │ Result     │ │  │ │ Result     │ │         │
│  │ │ State      │ │  │ │ State      │ │  │ │ State      │ │         │
│  │ │ Progress   │ │  │ │ Progress   │ │  │ │ Progress   │ │         │
│  │ └────────────┘ │  │ └────────────┘ │  │ └────────────┘ │         │
│  │ hEventStart    │  │ hEventStart    │  │ hEventStart    │         │
│  │ hEventComplete │  │ hEventComplete │  │ hEventComplete │         │
│  │ hEventCancel   │  │ hEventCancel   │  │ hEventCancel   │         │
│  └────────────────┘  └────────────────┘  └────────────────┘         │
│                                                                       │
│  Each worker runs: CompilerEngine_ExecutePipeline() asynchronously  │
│                                                                       │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                    CACHE LAYER (LRU)                                 │
├─────────────────────────────────────────────────────────────────────┤
│                                                                       │
│  Cache Entry → Cache Entry → Cache Entry → Cache Entry             │
│  (Hot)        (Recently)    (Used)        (Evicted)                 │
│                                                                       │
│  Size: 100 MB total                                                  │
│  Hit Rate: 60-85% typical                                            │
│  Thread Safe: Protected with mutex                                   │
│  Eviction: LRU when full                                             │
│                                                                       │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                   MEMORY MANAGEMENT LAYER                            │
├─────────────────────────────────────────────────────────────────────┤
│                                                                       │
│  Engine Heap (1 MB initial, growable)                               │
│  ├── COMPILER_ENGINE structure                                       │
│  ├── WORKER_CONTEXT arrays (4 workers)                              │
│  ├── Cache entries                                                   │
│  └── Diagnostic messages                                             │
│                                                                       │
│  Lexer Heap (Process heap)                                          │
│  ├── Token array                                                     │
│  └── Source buffer                                                   │
│                                                                       │
│  Result Heap (Process heap)                                         │
│  ├── COMPILE_RESULT structure                                        │
│  └── DIAGNOSTIC array                                                │
│                                                                       │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                  SYNCHRONIZATION LAYER                               │
├─────────────────────────────────────────────────────────────────────┤
│                                                                       │
│  hMutexWorkers     - Worker pool access protection                  │
│  hMutexCache       - Cache operations protection                    │
│  hMutexDiagnostics - Diagnostic reporting protection                │
│                                                                       │
│  hJobObject        - Process management & limits                    │
│  hIoCompletion     - Async I/O completion notification              │
│  hTimerQueue       - Statistics collection                          │
│                                                                       │
│  Worker Events:                                                       │
│  ├── hEventStart    - Signal to start compilation                   │
│  ├── hEventComplete - Completion notification                       │
│  └── hEventCancel   - Cancellation signal                           │
│                                                                       │
└─────────────────────────────────────────────────────────────────────┘
```

## Data Flow

```
User Code
    │
    ├─ Create Engine
    │  └──> CompilerEngine_Create()
    │      └──> Allocate private heap
    │      └──> Create 4 worker threads
    │      └──> Initialize synchronization
    │      └──> Return engine pointer
    │
    ├─ Setup Options
    │  └──> COMPILE_OPTIONS structure
    │      ├── Source path
    │      ├── Target architecture
    │      ├── Optimization level
    │      └── Language
    │
    ├─ Compile Source
    │  └──> CompilerEngine_Compile(engine, options)
    │      ├─ Check cache for hit
    │      │  └──> SHA-256 key lookup
    │      │  └──> Return cached result if found
    │      │
    │      └─ Cache miss → Execute pipeline
    │         ├─ [Stage 1] Lexing
    │         │  └──> Read source file
    │         │  └──> Tokenize into token stream
    │         │  └──> Report lexical errors
    │         │
    │         ├─ [Stage 2] Parsing
    │         │  └──> Build Abstract Syntax Tree
    │         │  └──> Report syntax errors
    │         │
    │         ├─ [Stage 3] Semantic Analysis
    │         │  └──> Type checking
    │         │  └──> Symbol resolution
    │         │  └──> Report semantic errors
    │         │
    │         ├─ [Stage 4] IR Generation
    │         │  └──> Generate intermediate representation
    │         │  └──> Build basic blocks
    │         │
    │         ├─ [Stage 5] Optimization
    │         │  └──> Constant folding
    │         │  └──> Dead code elimination
    │         │  └──> Variable propagation
    │         │
    │         ├─ [Stage 6] Code Generation
    │         │  └──> Target-specific codegen
    │         │  └──> Register allocation
    │         │  └──> Instruction selection
    │         │
    │         ├─ [Stage 7] Assembly
    │         │  └──> Generate object file
    │         │  └──> Symbol table generation
    │         │
    │         ├─ [Stage 8] Linking
    │         │  └──> Link object files
    │         │  └──> Generate executable
    │         │
    │         └─ Store result in cache
    │
    ├─ Check Result
    │  └──> if (success)
    │      ├── Use generated code
    │      ├── Check output files
    │      └── Record compilation time
    │  └──> else
    │      ├── Iterate diagnostics
    │      ├── Report errors to user
    │      └── Suggest fixes
    │
    ├─ Free Result
    │  └──> HeapFree(result)
    │
    └─ Destroy Engine
       └──> CompilerEngine_Destroy(engine)
           ├── Signal worker threads to stop
           ├── Wait for completion (5 sec timeout)
           ├── Force terminate if needed
           ├── Close all handles
           ├── Free all memory
           └── Cleanup synchronization objects
```

## Memory Layout

```
┌──────────────────────────────────────────────────────────┐
│              COMPILER_ENGINE Structure                   │
├──────────────────────────────────────────────────────────┤
│                                                           │
│  Offset  Field                   Size      Purpose       │
│  ────────────────────────────────────────────────────────│
│  0x000   magic                   4 bytes   Validation    │
│  0x004   signature               8 bytes   Security      │
│  0x00C   hHeap                   8 bytes   Allocator     │
│  0x014   hJobObject              8 bytes   Job control   │
│  0x01C   hIoCompletion           8 bytes   Async I/O     │
│  0x024   workers[64]             512 bytes Worker array  │
│  0x224   workerCount             4 bytes   Count         │
│  0x228   activeWorkers           4 bytes   Active count  │
│  0x22C   hMutexWorkers           8 bytes   Mutex         │
│  0x234   hMutexCache             8 bytes   Mutex         │
│  0x23C   hMutexDiagnostics       8 bytes   Mutex         │
│  0x244   cacheHead               8 bytes   LRU head      │
│  0x24C   cacheTail               8 bytes   LRU tail      │
│  0x254   cacheCount              4 bytes   Entry count   │
│  0x258   cacheSize               8 bytes   Total size    │
│  0x260   cacheMaxSize            8 bytes   Max size      │
│  ...    [Other fields]                                   │
│                                                           │
└──────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────┐
│              COMPILE_RESULT Structure                    │
├──────────────────────────────────────────────────────────┤
│                                                           │
│  Offset  Field                   Size      Purpose       │
│  ────────────────────────────────────────────────────────│
│  0x000   success                 4 bytes   0=fail 1=ok   │
│  0x004   fromCache               4 bytes   Cache flag    │
│  0x008   options                 ~2K       Options used  │
│  [+2K]   diagnostics             8 bytes   Diag array    │
│  [+2K+8] diagCount               4 bytes   Diag count    │
│  [+2K+C] outputFiles[16]         128 bytes Output paths  │
│  [+2K+8C]outputCount             4 bytes   File count    │
│  [+2K+90]startTime               8 bytes   Start time    │
│  [+2K+98]endTime                 8 bytes   End time      │
│  [+2K+A0]compilationTimeMs       8 bytes   Duration      │
│  [+2K+A8]lastStage               4 bytes   Last stage    │
│  [+2K+AC]objectCode              8 bytes   Code ptr      │
│  [+2K+B4]objectCodeSize          8 bytes   Code size     │
│  [+2K+BC]assemblyOutput[65536]   64K bytes Assembly text │
│  [+2K+...] statistics            ~64 bytes Stats struct  │
│                                                           │
└──────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────┐
│              CACHE_ENTRY Structure                       │
├──────────────────────────────────────────────────────────┤
│                                                           │
│  Offset  Field                   Size      Purpose       │
│  ────────────────────────────────────────────────────────│
│  0x000   key[64]                 64 bytes  Cache key     │
│  0x040   result                  ~70KB     Full result   │
│  [...+70K] timestamp             8 bytes   Last access  │
│  [...+70K+8] accessCount         8 bytes   Hit count    │
│  [...+70K+10] size               8 bytes   Entry size   │
│  [...+70K+18] valid              4 bytes   Is valid     │
│  [...+70K+1C] next               8 bytes   LRU next     │
│  [...+70K+24] prev               8 bytes   LRU prev     │
│                                                           │
│  Total Size: ~70 KB per entry                            │
│  Max Entries: 1024                                       │
│  Max Total: 100 MB                                       │
│                                                           │
└──────────────────────────────────────────────────────────┘
```

## Performance Profile

```
Operation              Time Complexity    Space Complexity
──────────────────────────────────────────────────────────
Create Engine          O(1)               O(1) - 2 MB
Destroy Engine         O(workers)         O(1)
Compile (no cache)     O(n) where n=bytes O(n)
Compile (cache hit)    O(log entries)     O(1)
Cache Lookup           O(1) average       O(1)
Cache Store            O(1) amortized     O(cache_entry)
Cache Evict            O(k) k=evict_count O(1)
Lexing                 O(n) linear        O(tokens)
Token Lookup           O(1) hash table    O(keywords)
String Compare         O(m) m=str_len     O(1)
Memory Allocate        O(1)               O(size)
Memory Deallocate      O(1)               O(1)

Legend:
- n = source code size
- m = string length
- entries = cache entries
- tokens = token count
```

## Thread Model

```
Main Thread                Worker Threads (4)
────────────────────────────────────────────────────────
│
├─ Create Engine
│  ├─ Create Worker 1 ──┐
│  ├─ Create Worker 2 ──┼─ Wait for start event
│  ├─ Create Worker 3 ──┤
│  └─ Create Worker 4 ──┘
│
├─ Compile Request
│  │
│  ├─ Check Cache
│  │  └─ Acquire hMutexCache
│  │     Search LRU list
│  │     Release hMutexCache
│  │     If hit: return immediately
│  │
│  └─ Find Worker
│     Acquire hMutexWorkers
│     Find inactive worker
│     Mark active
│     Release hMutexWorkers
│     │
│     ├─ Copy options to worker
│     │
│     └─ Signal hEventStart ────┐ Worker processes:
│                               │ 1. Lexing
│  Wait for hEventComplete ◄────┼─ 2. Parsing
│                               │ 3. Semantic
│  Copy result from worker      │ 4. IR Gen
│  Mark worker inactive         │ 5. Optimize
│                               │ 6. Codegen
│  Return result ───────────────┼─ 7. Assembly
│                               │ 8. Linking
│  Close/Destroy                │ Signal hEventComplete
│  Acquire hMutexWorkers        └─ Wait for next
│  Find worker
│  Terminate if needed
│  Release hMutexWorkers
```

## Cache Organization (LRU)

```
Time →

Most Recently Used
↓
[A] ← ptr → [B] ← ptr → [C] ← ptr → [D] ← ptr → [E]
Head (front)                                    Tail (end)
                                                ↓
                                    Least Recently Used


Cache States:

1. New Entry: A is moved to head
   [A] ← ptr → [B] ← ptr → [C]

2. Hit Entry: C is moved to head
   [C] ← ptr → [A] ← ptr → [B]

3. Full Cache: Evict tail (oldest)
   Remove [B], shrink head
   [C] ← ptr → [A]

4. Multiple Entries: LRU order maintained
   [Recent] ← ptr → ... ← ptr → [Oldest]
```

This architecture ensures:
- ✅ High-performance compilation
- ✅ Minimal memory footprint
- ✅ Efficient caching
- ✅ Safe parallel execution
- ✅ Comprehensive error reporting
- ✅ Clean resource management

