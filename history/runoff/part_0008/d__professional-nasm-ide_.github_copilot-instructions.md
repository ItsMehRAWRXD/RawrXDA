# Professional NASM IDE - AI Coding Agent Instructions

## 🏗️ Architecture Overview

**Professional NASM IDE** is a high-performance assembly language IDE written primarily in x64 NASM assembly with DirectX 11 integration and Python tooling. The project prioritizes native performance through direct assembly implementation while maintaining Python-based tooling for advanced features.

### Core Assembly Components (`src/`)
- **DirectX 11 Text Engine** (`directx_text_engine.asm`): GPU-accelerated text rendering with COM interface calls, GDI fallback
- **Ollama Native Wrapper** (`ollama_native.asm`): Pure ASM HTTP/1.1 client for LLM APIs (no libc/curl dependencies)
- **JSON Parser** (`json_parser.asm`): Native JSON parsing for Ollama responses
- **Multi-Language Bridge** (`language_bridge.asm`): FFI system for Python/Rust/C/C++ interop via ctypes
- **Extension Marketplace** (`extension_marketplace.asm`): Plugin system for language support
- **Model Dampener** (`model_dampener.asm`): Runtime AI model behavior modification
- **Text Editor Engine** (`text_editor_engine.asm`): Core editor with undo/redo, syntax highlighting, find/replace
- **File System** (`file_system.asm`): File I/O operations with GetOpenFileName/GetSaveFileName

### Python Tooling Layer (`tools/`)
- **Swarm Orchestrator** (`swarm_orchestrator.py`): Multi-agent AI system (10 specialized models, 200-400MB each)
- **Symbolic Executor** (`symbolic_executor.py`): Symbolic execution & theorem proving for assembly verification
- **Privacy Compliance** (`privacy_compliance_integration.py`): PII detection, GDPR/CCPA compliance checking
- **Self-Healing Supervisor** (`self_heal.py`): Process monitoring with auto-restart and health checks
- **Fuzz Generator** (`fuzz_generator.py`): Mutation-based fuzzing payload generation for testing

### Bridge Architecture
Python tools integrate with ASM core via `asm_bridge.py` (ctypes FFI):
```python
from asm_bridge import ASMBridge
bridge = ASMBridge()
if bridge.is_available():
    # Native ASM implementation (10-100x faster)
else:
    # Pure Python fallback
```

## 🎯 Critical Assembly Patterns

### Structure & Memory Layout
```nasm
BITS 64
DEFAULT REL  ; RIP-relative addressing required

struc OllamaConfig
    .host_ptr:    resq 1
    .port:        resw 1
    .socket_fd:   resd 1
    .size:
endstruc

; Access: [base + OllamaConfig.host_ptr]
```

### COM Interface Calls (DirectX)
Manual vtable navigation - **ALWAYS call Release()** to prevent leaks:
```nasm
; Get vtable pointer
mov rax, [rcx]              ; rcx = COM object pointer
; Call method at offset 0x18
call qword [rax + 0x18]     ; Direct vtable call
; Release COM object (offset 0x10 for IUnknown::Release)
mov rcx, [my_object]
mov rax, [rcx]
call qword [rax + 0x10]
```

### HTTP Client Implementation (Ollama)
Pure ASM HTTP/1.1 without external libraries:
```nasm
; Initialize & connect
call ollama_init                    ; config, host, host_len, port
call ollama_connect                 ; config

; Build & send request
call build_http_post                ; buffer, pos, capacity, endpoint, body, body_len
call send_win                       ; socket, buffer, length

; Parse response
call find_http_body                 ; Locate JSON in HTTP response
call json_parse_response            ; Extract JSON fields
```

### Buffer Safety (MANDATORY)
ALL buffer operations MUST use bounds checking:
```nasm
; append_string(buffer, pos, capacity, str, len) -> new_pos or -1
mov rcx, response_buffer
mov rdx, [buffer_pos]
mov r8, RESP_BUFFER_SIZE
mov r9, str_ptr
push str_len
call append_string
add rsp, 8
cmp rax, -1
je .buffer_overflow_error
```

### Cross-Platform Syscalls
```nasm
%ifdef WINDOWS
    ; Windows fastcall: RCX, RDX, R8, R9, stack
    extern WSAStartup
    call WSAStartup
%else
    ; Linux syscall registers: RAX=syscall# RDI RSI RDX R10 R8 R9
    mov rax, 41          ; socket syscall
    syscall
%endif
```


## 🔨 Build Workflows

### Primary Build Commands (Run from project root)
```batch
# Complete IDE with DirectX (PRIMARY)
BUILD-AND-LAUNCH.bat        # Builds & launches full IDE

# Component builds
build-asm-core.bat          # ASM modules only → lib/nasm_ide_core.dll
build-ollama.bat            # Ollama wrapper only
build-tests.bat             # Integration test suite

# DirectX IDE
build-dx-ide.bat            # DirectX text engine build
```

### Build Order & Dependencies
1. **Core ASM modules** (`build-asm-core.bat`) - Creates shared library first
2. **Main executable** (`BUILD-AND-LAUNCH.bat`) - Links against core + DirectX libraries
3. **Tests** (`build-tests.bat`) - Requires core modules compiled

### Key Build Outputs
```
bin/
  nasm_ide_dx.exe           # Main IDE executable
lib/
  nasm_ide_core.dll         # Native ASM implementations (Windows)
  nasm_ide_core.so          # Native ASM implementations (Linux)
build/
  *.obj                     # Object files
  fuzz_payloads/            # Generated test payloads
```

### Python Environment Setup
```powershell
# Create virtual environment
python -m venv .venv
.venv\Scripts\Activate.ps1

# Install Python tooling dependencies
pip install -r tools/requirements.txt

# Run Python tools
python tools/swarm_orchestrator.py --config swarm_config.json
python tools/symbolic_executor.py src/ollama_native.asm --check-unreachable
python tools/privacy_compliance_integration.py --workspace . --compliance
```


## 🧪 Testing & Debugging Workflows

### Assembly Testing
```batch
# ABI violation detection
test_abi_violations.bat

# Bridge system tests
nasm -f win64 test_bridge_init.asm -o build/test_bridge.obj
link /OUT:bin/test_bridge.exe build/test_bridge.obj

# Ollama integration tests
build-tests.bat             # Compiles all 4 integration tests:
                           # - test_append_string
                           # - test_find_body
                           # - test_parse_response
                           # - test_cached_headers
```

### Python Tool Testing
```python
# Self-healing supervisor test
python tools/self_heal.py --config tools/self_heal_config.json

# Symbolic execution on assembly
python tools/symbolic_executor.py src/ollama_native.asm --max-paths 100 --check-unreachable

# Fuzzing tests
python tools/fuzz_generator.py          # Generates 256 payloads
python tests/fuzz_ollama.py             # Runs fuzzing against Ollama wrapper
```

### Debug Outputs & Logs
- **Build Logs**: `build_errors.txt` - Compilation error output
- **Runtime Logs**: `nasm_ide.log` - IDE runtime debug output
- **Performance**: `performance_validation_report.json` - Benchmark results
- **Self-Heal**: Process monitoring output in console

### Common Debug Patterns
```nasm
; Debug logging in assembly (Windows)
extern printf
section .data
    debug_msg db "Debug: rax=%lld", 10, 0
section .text
    sub rsp, 40         ; Shadow space + alignment
    lea rcx, [debug_msg]
    mov rdx, rax
    call printf
    add rsp, 40
```


## 🔌 Integration Patterns

### Multi-Language Bridge (ASM ↔ Python)
```python
# From Python, call native ASM functions
from asm_bridge import ASMBridge
bridge = ASMBridge()

if bridge.is_available():
    # Extract model profile (native implementation)
    profile = bridge.extract_profile("path/to/model.gguf")
    
    # Register extension (native marketplace)
    idx = bridge.register_extension(
        "Python Language Support",
        bridge.LANG_PYTHON,
        bridge.CAP_SYNTAX_HIGHLIGHT | bridge.CAP_DEBUGGING
    )
else:
    # Pure Python fallback when DLL unavailable
    profile = python_extract_profile("path/to/model.gguf")
```

### Swarm Multi-Agent Coordination
```python
# Load swarm configuration
swarm = SwarmController("swarm_config.json")
swarm.start()

# Submit task to specialized agents
task = Task(
    id="syntax_check_001",
    type="syntax_analysis",
    data={"code": asm_code, "language": "nasm"}
)
result = await swarm.submit_task(task)
```

### Ollama LLM Integration (from ASM)
```nasm
; Initialize connection
lea rcx, [config]
lea rdx, [hostname]
mov r8, hostname_len
mov r9w, 11434
call ollama_init

; Connect to server
lea rcx, [config]
call ollama_connect

; Generate completion
lea rcx, [config]
lea rdx, [request]      ; OllamaRequest structure
lea r8, [response]      ; OllamaResponse structure
call ollama_generate
```

### Privacy & Compliance Integration
```python
# Full privacy audit workflow
auditor = PrivacyComplianceIntegration(Path("."))

# Scan for PII
report = auditor.full_privacy_audit()

# Check GDPR/CCPA compliance
compliance = auditor.check_compliance(Path("privacy_config.json"))

# Auto-remediate detected PII
scan_results = auditor.privacy_auditor.scan_directory(Path("."))
auditor.auto_remediate(scan_results)
```


## 📋 Code Style Conventions

### Assembly (NASM)
- **BITS 64** at top, **DEFAULT REL** for RIP-relative addressing
- Functions: `snake_case` (e.g., `ollama_init`, `build_http_post`)
- Structures: `PascalCase` (e.g., `OllamaConfig`, `OllamaRequest`)
- Error handling: `0`=success, `-1` or non-zero=failure
- **No RBP stack frames** (intentional ABI violation for performance - see `ABI_VIOLATIONS_COMPLETELY_FIXED.md`)
- Always use `extern` declarations for cross-module calls
- Comments: `;` for inline, `; ===` separator lines for major sections

### Python Tooling
- Type hints for all function signatures
- Dataclasses for structured data
- Async/await for I/O-bound operations (swarm coordination)
- Comprehensive docstrings with usage examples
- Logging via `logging` module, not print statements
- Config files in JSON format (e.g., `swarm_config.json`, `privacy_config.json`)

### Structure Organization
```nasm
; File header with purpose
BITS 64
DEFAULT REL

%ifdef WINDOWS
    ; Platform-specific includes
%endif

; Structure definitions
struc MyStruct
    .field1: resq 1
    .size:
endstruc

; Constants
%define BUFFER_SIZE 16384

; Sections
section .bss
section .data
section .text

; Functions (global exports first)
global my_function
my_function:
    ; Implementation
```


## 🚀 Performance Optimizations

### Assembly Core
- **GPU-accelerated text rendering**: DirectX 11 with DirectWrite for text layout
- **Zero-copy socket operations**: Raw buffer passing to WinSock APIs
- **Fixed buffers**: 64KB text buffer, 32KB undo/redo stacks, 128KB response buffer
- **Static header caching**: Pre-built HTTP headers in `g_common_headers` (512 bytes)
- **Text layout caching**: Reuse DirectWrite text layouts when content unchanged
- **Inline assembly hot paths**: Critical functions in ASM, not C wrappers

### Python Tools
- **Parallel processing**: `ThreadPoolExecutor` for multi-agent swarm tasks
- **Async I/O**: `asyncio`/`aiohttp` for network-bound operations
- **Lazy loading**: Models loaded on-demand, not at startup
- **Memory-efficient fuzzing**: Streaming payload generation, not in-memory sets
- **Z3 SMT solver integration**: Symbolic execution uses constraint solving for path feasibility (see `symbolic_executor.py`)

### Memory Layout
```nasm
; Minimize cache misses - group related data
struc OllamaConfig
    .host_ptr:      resq 1   ; 8 bytes
    .host_len:      resq 1   ; 8 bytes
    .port:          resw 1   ; 2 bytes
    .timeout_ms:    resq 1   ; 8 bytes
    .socket_fd:     resd 1   ; 4 bytes
    .connected:     resb 1   ; 1 byte (flags together)
    .keepalive:     resb 1
    .retry_count:   resb 1
    .padding:       resb 1   ; Align to 8-byte boundary
    .size:
endstruc
```


## 🔍 Key Reference Files

### Architecture & Design
- `ASM-CORE-README.md`: Native ASM implementation architecture
- `FINAL-COMPLETION-REPORT.md`: DirectX 11 feature implementation status (22/22 todos)
- `1001-PERCENT-COMPLETE.md`: Ollama wrapper completion report (6/6 todos)
- `ABI_VIOLATIONS_COMPLETELY_FIXED.md`: Performance optimizations via ABI violations

### Implementation Examples
- `src/ollama_native.asm`: HTTP/1.1 client, socket operations, JSON integration (582 lines)
- `src/directx_text_engine.asm`: COM interface patterns, DirectX usage
- `src/json_parser.asm`: JSON parsing without external libraries (337 lines)
- `tools/swarm_orchestrator.py`: Multi-agent AI coordination (584 lines)
- `tools/symbolic_executor.py`: Assembly verification & theorem proving (584 lines)

### Testing & Validation
- `tests/test_ollama_integration.asm`: 4 integration tests (append_string, find_body, parse_response, cached_headers)
- `tools/fuzz_generator.py`: Mutation-based fuzzing (256 payloads)
- `tools/self_heal.py`: Process supervision with health checks (247 lines)
- `tools/privacy_compliance_integration.py`: GDPR/CCPA compliance automation (216 lines)

### Build & Configuration
- `BUILD-AND-LAUNCH.bat`: Primary build script (compiles dx_ide_main → nasm_ide_dx.exe)
- `build-asm-core.bat`: Shared library build (model_dampener, extension_marketplace, language_bridge)
- `build-tests.bat`: Test suite compilation
- `swarm_config.json`: 10-model swarm configuration (code completion, syntax analysis, etc.)


## ⚠️ Critical Recovery Guidelines

### Assembly Code Recovery Best Practices
- **NEVER** do large bulk code removals in assembly - labels and offsets are fragile.
- **ALWAYS** compile after each significant change to catch errors early.
- **Keep old code** commented out with TODO/DEPRECATED markers before deletion.
- **Add alongside, not replace**: Implement new functions beside old ones, test, then switch.
- **Backup workflow**: Test in `_v2.asm` files, verify build, then replace main file.
- **Incremental integration**: Add extern declarations, implement wrappers, test each step.
- If 100+ errors appear: STOP, backup current file, start from clean minimal version.

### Example Recovery Workflow
```bash
# 1. Backup corrupted file
cp src/ollama_native.asm src/ollama_native_corrupted_backup.asm

# 2. Create clean minimal version
cp src/ollama_native_v2.asm src/ollama_native.asm

# 3. Compile to verify
build-ollama.bat

# 4. Incrementally add features back
# Edit src/ollama_native.asm - add ONE function
build-ollama.bat
# Repeat until all features restored
```

## ⚠️ Common Pitfalls

### Assembly Development
- **COM objects**: Always `Release()` to prevent memory leaks
- **Syscalls**: Windows fastcall (RCX, RDX, R8, R9) vs Linux syscall (RAX, RDI, RSI, RDX)
- **Memory alignment**: 16-byte alignment required for SSE/AVX operations
- **Build order**: Core ASM modules must compile before main executable
- **Buffer overflows**: ALL buffer operations MUST use bounds-checked `append_string`
- **Label dependencies**: Changing label names breaks jump/call references silently

### Python Tools
- **Model loading**: 10 models @ 200-400MB each = 2-4GB total, load on-demand only
- **Swarm coordination**: Use async/await for I/O, ThreadPoolExecutor for CPU work
- **Symbolic execution**: Z3 SMT solver required for constraint solving, install separately
- **Privacy scanning**: PII detection can have false positives, always review before remediation

### Cross-Platform Issues
```nasm
; WRONG - hardcoded Windows
extern WSAStartup
call WSAStartup

; RIGHT - platform-conditional
%ifdef WINDOWS
    extern WSAStartup
    call WSAStartup
%else
    mov rax, 41    ; Linux socket syscall
    syscall
%endif
```


## 🤖 GitHub Copilot Extension Integration

### Copilot Context Optimization

**Assembly-Specific Context Patterns**
- Keep structure definitions visible in the same file as usage
- Include relevant completion reports in workspace for historical context
- Open related test files alongside implementation for better suggestions
- Use descriptive variable names even in assembly (e.g., `http_response_buffer` vs `buf1`)

**Multi-File Context Strategy**
```nasm
; GOOD - Copilot can infer from open files
; File 1 (open): src/json_parser.asm with parse_json_object function
; File 2 (open): src/ollama_native.asm
; Copilot suggestion: Use parse_json_object for response parsing

; BAD - Copilot lacks context
; Only src/ollama_native.asm open
; Copilot suggestion: Inline JSON parsing (duplicates existing code)
```

### Prompt Engineering for Assembly

**Effective Comment Prompts**
```nasm
; TODO: Parse JSON response and extract "model" field using existing json_parser functions
; Expected structure: {"model": "llama2", "created_at": "..."};
; Return value: pointer to model string or NULL on error
parse_model_from_response:
    ; [Copilot generates implementation using parse_json_object]
```

**Structured Request Pattern**
```nasm
; FUNCTION: send_http_request
; INPUTS: RCX=socket_fd, RDX=request_buffer, R8=buffer_len
; OUTPUTS: RAX=bytes_sent (-1 on error)
; USES: send_win, append_string, build_http_post
; NOTES: Must handle partial sends, validate buffer bounds
send_http_request:
    ; [Copilot generates with proper ABI, error handling, bounds checks]
```

### Copilot-Assisted Development Workflows

**Assembly Code Generation**
```nasm
; 1. Define structure with detailed comments
struc DirectXTextMetrics
    .width:         resd 1  ; Text width in pixels
    .height:        resd 1  ; Text height in pixels
    .baseline:      resd 1  ; Baseline offset from top
    .line_count:    resd 1  ; Number of lines
    .size:
endstruc

; 2. Write function signature with intent
; TODO: Calculate text metrics using DirectWrite's IDWriteTextLayout::GetMetrics
; Must call COM methods via vtable, handle HRESULT errors, fill DirectXTextMetrics
get_text_metrics:
    ; [Copilot generates COM interface calls with proper vtable offsets]
```

**Python Tool Integration**
```python
# Open both asm_bridge.py and new tool file
# Copilot infers FFI patterns automatically

from asm_bridge import ASMBridge
from pathlib import Path

class AssemblyProfiler:
    """Profile assembly code execution using native instrumentation hooks."""

    def __init__(self):
        self.bridge = ASMBridge()
        # [Copilot suggests: check if native profiling available]
        if not self.bridge.is_available():
            raise RuntimeError("Native profiling requires ASM core library")

    def profile_function(self, func_name: str, iterations: int = 1000):
        # [Copilot suggests: call bridge.profile_execution(func_name, iterations)]
```

### Context Files for Better Suggestions

**Always Keep Open**
1. Target implementation file (e.g., `src/ollama_native.asm`)
2. Related structure definitions (may be in same file or separate)
3. Relevant test file (`tests/test_ollama_integration.asm`)
4. Completion report for feature area (`1001-PERCENT-COMPLETE.md`)
5. This instructions file (`.github/copilot-instructions.md`)

**Workspace Configuration**
```json
// .vscode/settings.json
{
  "github.copilot.advanced": {
    "contextFiles": [
      "src/**/*.asm",
      "tests/**/*.asm",
      ".github/copilot-instructions.md",
      "*-COMPLETE.md",
      "swarm_config.json"
    ]
  }
}
```

### Copilot Chat Patterns

**Assembly Review Request**
```
@workspace Review this assembly function for:
1. ABI compliance (Windows x64 fastcall)
2. Buffer overflow vulnerabilities
3. Missing error handling
4. Memory leak risks (COM objects)

#file:src/directx_text_engine.asm
```

**Cross-Reference Query**
```
@workspace How should I call the existing JSON parser from ollama_native.asm?
Show me examples from test files.

Context: #file:src/json_parser.asm #file:tests/test_ollama_integration.asm
```

**Integration Pattern Request**
```
Generate Python wrapper for the new ASM function in extension_marketplace.asm
that follows the pattern in asm_bridge.py

Requirements:
- Type hints
- Error handling with fallback
- Docstring with usage example
```

### Avoiding Common Copilot Pitfalls

**Assembly-Specific Issues**
```nasm
; PITFALL: Copilot suggests standard library functions
; BAD SUGGESTION
extern strlen
call strlen

; CORRECT - Use existing bounds-checked utilities
lea rcx, [buffer]
lea rdx, [str_ptr]
mov r8, [str_len]
call append_string  ; Already implements length handling
```

**Platform-Conditional Code**
```nasm
; PITFALL: Copilot suggests Windows-only code
; BAD SUGGESTION
call WSAStartup

; GOOD - Request platform-conditional explicitly
; TODO: Initialize socket subsystem (Windows: WSAStartup, Linux: no-op)
%ifdef WINDOWS
    ; [Copilot generates WSAStartup call]
%else
    ; [Copilot generates Linux alternative or no-op]
%endif
```

**Memory Management**
```nasm
; PITFALL: Copilot forgets COM Release()
; BAD SUGGESTION
mov rcx, [d3d_device]
mov rax, [rcx]
call qword [rax + 0x18]  ; CreateTexture2D
; Missing Release()!

; GOOD - Explicitly request cleanup
; TODO: Create DirectX texture and ALWAYS Release() when done
; Pattern: Get interface -> Call method -> Release interface
```

### Integration with Symbolic Executor

**Request Verification-Friendly Code**
```nasm
; TODO: Implement buffer append with symbolic execution verification in mind
; Requirements:
; - All paths must terminate (no infinite loops)
; - All error conditions explicit (no implicit failures)
; - All buffer accesses bounds-checked
; - Return values: new_position (success) or -1 (failure)
append_with_verification:
    ; [Copilot generates verifiable code following symbolic_executor.py patterns]
```

**Theorem Proving Hints**
```python
# Request Copilot to generate with Z3 assertions
# TODO: Add symbolic execution test for new append_string_utf16 function
# Verify properties:
# 1. No buffer overflows (pos + len <= capacity)
# 2. Return value correctness (new_pos == old_pos + len OR new_pos == -1)
# 3. No uninitialized memory reads

def test_append_string_utf16_symbolic():
    # [Copilot generates Z3 constraint solving test]
```

### Swarm Orchestrator Integration

**Multi-Agent Code Review Pattern**
```python
# Open swarm_orchestrator.py and new code file
# Request distributed code review

# TODO: Submit assembly function to swarm for multi-perspective review
# Agents needed: syntax_analyzer, security_auditor, performance_optimizer
async def review_assembly_with_swarm(code_path: Path) -> dict:
    """Use swarm agents for comprehensive assembly code review."""
    # [Copilot suggests SwarmController usage with proper task distribution]
```

### Quick Reference: Copilot Commands

**Assembly Development**
```bash
# In editor, trigger Copilot with:
# - Detailed function comments (inputs/outputs/constraints)
# - Open related files (parser, tests, structures)
# - Use TODO comments with explicit requirements

# Example workflow:
# 1. Open src/json_parser.asm, src/ollama_native.asm
# 2. Write: ; TODO: Parse JSON array of strings using parse_json_array
# 3. Tab to accept Copilot suggestion
# 4. Build with build-ollama.bat to validate
```

**Python Tooling**
```python
# 1. Open asm_bridge.py (for FFI patterns)
# 2. Open symbolic_executor.py (for verification patterns)
# 3. Open swarm_orchestrator.py (for multi-agent patterns)
# 4. Write class/function signature with comprehensive docstring
# 5. Let Copilot generate implementation following established patterns
```

**Debugging with Copilot**
```
# Copilot Chat queries for debugging:
@workspace Why does this assembly function crash after Release()?
#file:src/directx_text_engine.asm

@workspace Compare my HTTP parsing with the working implementation
#file:src/ollama_native.asm #file:tests/test_find_body.asm

@workspace Generate symbolic execution test for this buffer operation
```

### Advanced Copilot Techniques

**Incremental Refinement**
```nasm
; Step 1: Get basic structure from Copilot
; TODO: Socket initialization for Windows
init_socket:
    ; [Copilot generates basic WSAStartup]

; Step 2: Refine with constraints
; TODO: Socket initialization with error handling, timeout config, keepalive
; Return: socket_fd (success) or -1 (failure)
; Must: Check WSAStartup HRESULT, handle socket() failures, set SO_KEEPALIVE
init_socket_robust:
    ; [Copilot generates comprehensive implementation]
```

**Pattern Replication**
```nasm
; Open working example + new file needing similar pattern
; File 1: src/ollama_native.asm (has send_win function)
; File 2: src/http_client.asm (needs recv_win function)

; TODO: Implement recv_win following same error handling pattern as send_win
; Must match: return value semantics, error codes, buffer validation
recv_win:
    ; [Copilot replicates send_win pattern for receive operation]
```

**Cross-Language Consistency**
```python
# Open ASM implementation + Python wrapper
# Copilot maintains consistent interfaces

# src/model_dampener.asm
# global dampen_model_output  ; RCX=model_ptr, RDX=threshold -> RAX=status

# tools/asm_bridge.py
def dampen_model_output(self, model_ptr: int, threshold: float) -> int:
    """
    Dampen model output using native ASM implementation.
    # [Copilot generates ctypes call matching ASM signature exactly]
    """
```

## 🎯 Development Workflow

### Complete Feature Development Cycle
1. **Plan**: Review relevant completion reports (`1001-PERCENT-COMPLETE.md`, `FINAL-COMPLETION-REPORT.md`)
2. **Design**: Check existing patterns in `src/` for similar implementations
3. **Implement**: Add new ASM function alongside existing code (don't replace)
4. **Test**: Compile with relevant build script (`build-asm-core.bat`, `BUILD-AND-LAUNCH.bat`)
5. **Validate**: Run integration tests (`build-tests.bat`)
6. **Document**: Update this file with new patterns discovered

### Assembly Feature Addition
```bash
# 1. Create test first
echo "global test_new_feature" > tests/test_new_feature.asm

# 2. Implement in src/
# Add function to relevant .asm file with extern declarations

# 3. Build & test
build-asm-core.bat
build-tests.bat

# 4. Check logs
cat build_errors.txt
cat nasm_ide.log
```

### Python Tool Development
```bash
# 1. Setup environment
python -m venv .venv
.venv\Scripts\Activate.ps1
pip install -r tools/requirements.txt

# 2. Create tool with docstrings & type hints
# Follow patterns in tools/symbolic_executor.py

# 3. Test standalone
python tools/my_new_tool.py --help

# 4. Integrate with swarm
# Add to swarm_config.json if needed
```

### Debugging Assembly Issues
```nasm
; 1. Add printf debugging
extern printf
section .data
    dbg_msg db "Debug: RAX=%lld RCX=%lld", 10, 0
section .text
    sub rsp, 40
    lea rcx, [dbg_msg]
    mov rdx, rax
    mov r8, rcx
    call printf
    add rsp, 40

; 2. Check nasm_ide.log for runtime errors
; 3. Use test_abi_violations.bat to catch ABI issues
; 4. Build with build-tests.bat for integration validation
```