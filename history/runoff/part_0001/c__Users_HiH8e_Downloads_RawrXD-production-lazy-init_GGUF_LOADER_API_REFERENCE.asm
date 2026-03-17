; GGUF Model Loader - Complete API Reference
; Pure MASM64 Implementation - Ready for Production Use

;=============================================================================
; MODULE: ml_masm.asm (Core Model Loader)
;=============================================================================

; PUBLIC: ml_masm_init(model_path: rcx, flags: rdx) -> eax
; 
; Load and parse GGUF model file
; - Opens file with read access
; - Creates file mapping
; - Maps file into memory
; - Parses GGUF header
; - Initializes rawr1024 engine
; - Calls parse_gguf_metadata_kv() for architecture extraction
; - Calls populate_tensor_cache() for tensor initialization
;
; IN:  rcx = pointer to null-terminated file path
;      rdx = flags (reserved, use 0)
; OUT: eax = 1 on success, 0 on failure
; 
; Example:
;   lea rcx, model_path
;   xor rdx, rdx
;   call ml_masm_init
;   test eax, eax
;   jz load_error

PUBLIC ml_masm_init
EXTERN ml_masm_init:PROC


; PUBLIC: ml_masm_inference(prompt: rcx) -> eax
;
; Execute inference on loaded model
; - Validates model is loaded
; - Passes prompt to rawr1024_process()
; - Stores result in response buffer
; - Returns 1 on success
;
; IN:  rcx = pointer to null-terminated prompt string
; OUT: eax = 1 on success, 0 on failure
;      Response available via ml_masm_get_response()
;
; Example:
;   lea rcx, prompt_string
;   call ml_masm_inference
;   test eax, eax
;   jz inference_error

PUBLIC ml_masm_inference
EXTERN ml_masm_inference:PROC


; PUBLIC: ml_masm_get_arch() -> rax
;
; Retrieve architecture information string
; - Returns pointer to formatted architecture string
; - Format: "GGUF v3: L32/H4096/H32/V32000"
; - Contains layer count, hidden size, vocab size, etc.
; - Valid only after ml_masm_init() succeeds
;
; IN:  (none)
; OUT: rax = pointer to 256-byte architecture string
;      NULL if no model loaded
;
; Example:
;   call ml_masm_get_arch
;   test rax, rax
;   jz no_arch
;   mov rcx, rax
;   call OutputDebugStringA

PUBLIC ml_masm_get_arch
EXTERN ml_masm_get_arch:PROC


; PUBLIC: ml_masm_get_tensor(tensor_name: rcx) -> rax
;
; Lookup tensor information by name
; - Searches g_tensor_cache for matching tensor name
; - Case-sensitive string comparison
; - Returns TENSOR_INFO structure on match
;
; IN:  rcx = pointer to null-terminated tensor name string
;      Examples: "token_embd.weight", "blk.0.attn.q.weight"
; OUT: rax = pointer to TENSOR_INFO structure on success
;      rax = NULL if tensor not found
;
; TENSOR_INFO structure (64 bytes):
;   +0:  name_str[64]          - Tensor name
;   +64: shape[4] (DWORD each) - Dimension sizes
;   +80: dtype (DWORD)         - Data type
;   +84: strides[4] (DWORD ea) - Memory strides
;   +100: data_ptr (QWORD)     - Data pointer
;   +108: tensor_size (QWORD)  - Size in bytes
;   +116: quant_level (DWORD)  - Quantization level
;
; Example:
;   lea rcx, "token_embd.weight"
;   call ml_masm_get_tensor
;   test rax, rax
;   jz tensor_not_found
;   ; rax now points to TENSOR_INFO

PUBLIC ml_masm_get_tensor
EXTERN ml_masm_get_tensor:PROC


; PUBLIC: ml_masm_get_response(buffer: rcx, max_len: rdx) -> rax
;
; Retrieve last inference response
; - Copies response to user-provided buffer
; - Null-terminates result
; - Returns length of response
;
; IN:  rcx = pointer to output buffer
;      rdx = maximum bytes to copy
; OUT: rax = length of response (without null terminator)
;
; Example:
;   lea rcx, response_buf
;   mov rdx, 8192
;   call ml_masm_get_response
;   ; rax = response length

PUBLIC ml_masm_get_response
EXTERN ml_masm_get_response:PROC


; PUBLIC: ml_masm_last_error() -> rax
;
; Retrieve last error message
; - Returns pointer to error description
; - Valid only if previous operation failed
; - Max 512 characters
;
; IN:  (none)
; OUT: rax = pointer to error string
;
; Example:
;   call ml_masm_init
;   test eax, eax
;   jnz success
;   call ml_masm_last_error
;   mov rcx, rax
;   call OutputDebugStringA

PUBLIC ml_masm_last_error
EXTERN ml_masm_last_error:PROC


; PUBLIC: ml_masm_free()
;
; Free model resources
; - Unmaps file from memory
; - Closes file mapping
; - Closes file handle
; - Clears loaded flag
;
; IN:  (none)
; OUT: (none)
;
; Example:
;   call ml_masm_free

PUBLIC ml_masm_free
EXTERN ml_masm_free:PROC

;=============================================================================
; MODULE: gguf_metadata_parser.asm (RFC-Compliant Metadata Extraction)
;=============================================================================

; PUBLIC: parse_gguf_metadata_complete(
;     mapped_data: rcx,
;     file_size: rdx,
;     metadata_count: r8,
;     out_arch: r9
; ) -> eax
;
; Extract architecture metadata from GGUF v3 file
; - Parses metadata KV entries starting at offset 24
; - Handles 12 different value types (u8, i8, u16, i16, u32, i32, f32, u64, i64, f64, bool, string)
; - Extracts critical architecture parameters
; - Stores results in MODEL_ARCH structure
; - Safe bounds checking against file size
;
; IN:  rcx = pointer to memory-mapped GGUF file data
;      rdx = total file size in bytes
;      r8  = number of metadata KV entries
;      r9  = pointer to MODEL_ARCH output structure
; OUT: eax = 1 on success, 0 on error
;
; Extracted fields:
;   MODEL_ARCH.num_layers           <- llama.block_count
;   MODEL_ARCH.hidden_size          <- llama.embedding_length
;   MODEL_ARCH.num_attention_heads  <- llama.attention.head_count
;   MODEL_ARCH.max_seq_length       <- llama.context_length
;   MODEL_ARCH.vocab_size           <- tokenizer.ggml.vocab_size
;   MODEL_ARCH.ffn_hidden_size      <- llama.feed_forward_length
;   MODEL_ARCH.rope_freq_base       <- llama.rope.freq_base
;
; Example:
;   mov rcx, mapped_data
;   mov rdx, file_size
;   mov r8,  metadata_count
;   lea r9,  g_model_arch
;   call parse_gguf_metadata_complete
;   test eax, eax
;   jz parse_error

PUBLIC parse_gguf_metadata_complete
EXTERN parse_gguf_metadata_complete:PROC


; PUBLIC: format_arch_string(
;     arch: rcx,
;     out_buffer: rdx,
;     max_len: r8d
; ) -> rax
;
; Format MODEL_ARCH into human-readable string
; - Converts numeric fields to decimal strings
; - Creates compact representation
; - Example output: "Llama: 32L/4096H/32H/32000V"
;
; IN:  rcx = pointer to MODEL_ARCH structure
;      rdx = output buffer
;      r8d = max bytes to write
; OUT: rax = length of formatted string
;
; Example:
;   lea rcx, g_model_arch
;   lea rdx, arch_string_buf
;   mov r8d, 512
;   call format_arch_string

PUBLIC format_arch_string
EXTERN format_arch_string:PROC

;=============================================================================
; MODULE: agent_chat_integration.asm (UI Integration Layer)
;=============================================================================

; PUBLIC: agent_chat_load_model(model_path: rcx) -> eax
;
; Load model with full UI integration
; - Loads GGUF file via ml_masm_init()
; - Extracts and displays architecture
; - Shows tensor information
; - Updates chat session state
; - Displays model info in UI
;
; IN:  rcx = pointer to model file path
; OUT: eax = 1 on success, 0 on failure
;
; Side Effects:
;   - Displays architecture info to console
;   - Displays tensor list
;   - Populates CHAT_SESSION_STATE
;
; Example:
;   lea rcx, "C:\models\llama-7b.gguf"
;   call agent_chat_load_model
;   test eax, eax
;   jz load_error

PUBLIC agent_chat_load_model
EXTERN agent_chat_load_model:PROC


; PUBLIC: agent_chat_run_inference(prompt: rcx) -> eax
;
; Execute inference and display result in chat UI
; - Validates model is loaded
; - Displays prompt to console
; - Runs ml_masm_inference()
; - Displays response
; - Updates chat history
; - Increments inference counter
;
; IN:  rcx = pointer to null-terminated prompt string
; OUT: eax = 1 on success, 0 on failure
;
; Side Effects:
;   - Updates CHAT_SESSION_STATE.inference_count
;   - Updates CHAT_SESSION_STATE.chat_history
;   - Updates CHAT_SESSION_STATE.last_inference_time
;
; Example:
;   lea rcx, "What is machine learning?"
;   call agent_chat_run_inference
;   test eax, eax
;   jz inference_error

PUBLIC agent_chat_run_inference
EXTERN agent_chat_run_inference:PROC


; PUBLIC: agent_chat_get_session_state() -> rax
;
; Get current chat session state
; - Returns pointer to CHAT_SESSION_STATE structure
; - Contains all session metadata
; - Valid only after loading a model
;
; IN:  (none)
; OUT: rax = pointer to CHAT_SESSION_STATE structure
;
; CHAT_SESSION_STATE fields:
;   +0:    model_loaded         - 1 if model loaded, 0 otherwise
;   +4:    model_name[256]      - Name of loaded model
;   +260:  model_path[512]      - File path of model
;   +772:  arch_string[512]     - Architecture info
;   +1284: layer_count          - Number of layers
;   +1288: hidden_size          - Hidden dimension
;   +1292: attention_heads      - Number of attention heads
;   +1296: vocab_size           - Vocabulary size
;   +1300: max_seq_length       - Max sequence length
;   +1304: tensor_count         - Total tensors
;   +1308: inference_count      - Number of inferences run
;
; Example:
;   call agent_chat_get_session_state
;   mov rcx, rax
;   mov eax, [rcx + CHAT_SESSION_STATE.inference_count]

PUBLIC agent_chat_get_session_state
EXTERN agent_chat_get_session_state:PROC


; PUBLIC: agent_chat_is_model_loaded() -> eax
;
; Check if model is currently loaded
;
; IN:  (none)
; OUT: eax = 1 if loaded, 0 if not
;
; Example:
;   call agent_chat_is_model_loaded
;   test eax, eax
;   jz no_model

PUBLIC agent_chat_is_model_loaded
EXTERN agent_chat_is_model_loaded:PROC


; PUBLIC: agent_chat_get_inference_count() -> rax
;
; Get total number of inferences performed
;
; IN:  (none)
; OUT: rax = inference count
;
; Example:
;   call agent_chat_get_inference_count
;   ; rax = number of inferences

PUBLIC agent_chat_get_inference_count
EXTERN agent_chat_get_inference_count:PROC

;=============================================================================
; MODULE: test_gguf_loader.asm (Test Suite)
;=============================================================================

; PUBLIC: test_gguf_loader_main()
;
; Execute comprehensive GGUF loader test suite
; - Tests loading multiple GGUF models
; - Validates header parsing
; - Tests metadata extraction
; - Tests tensor cache population
; - Tests all accessor APIs
; - Prints detailed results
;
; IN:  (none)
; OUT: (none, results printed to debug console)
;
; Example:
;   call test_gguf_loader_main

PUBLIC test_gguf_loader_main
EXTERN test_gguf_loader_main:PROC

;=============================================================================
; MODULE: model_loader_integration.asm (Master Orchestration)
;=============================================================================

; PUBLIC: model_loader_init() -> eax
;
; Initialize model loader system
; - Must be called once before other functions
; - Initializes global state
; - Sets up performance metrics
;
; IN:  (none)
; OUT: eax = 1 on success (always succeeds)
;
; Example:
;   call model_loader_init

PUBLIC model_loader_init
EXTERN model_loader_init:PROC


; PUBLIC: model_loader_load_gguf_model(model_path: rcx) -> eax
;
; Load GGUF model with full orchestration
; - Calls ml_masm_init() for file loading
; - Calls parse_gguf_metadata_complete() for metadata
; - Calls populate_tensor_cache() for tensor info
; - Calls agent_chat_load_model() for UI display
; - Tracks load time metrics
; - Records all errors
;
; IN:  rcx = pointer to model file path
; OUT: eax = 1 on success, 0 on failure
;
; Example:
;   lea rcx, model_path
;   call model_loader_load_gguf_model

PUBLIC model_loader_load_gguf_model
EXTERN model_loader_load_gguf_model:PROC


; PUBLIC: model_loader_run_inference(prompt: rcx) -> eax
;
; Run inference with performance tracking
; - Validates model is loaded
; - Records start time
; - Calls ml_masm_inference()
; - Tracks inference duration
; - Updates performance metrics
;   - Inference count
;   - Total/average/min/max inference times
; - Calls agent_chat_run_inference() for UI
;
; IN:  rcx = pointer to prompt string
; OUT: eax = 1 on success, 0 on failure
;
; Example:
;   lea rcx, prompt_text
;   call model_loader_run_inference

PUBLIC model_loader_run_inference
EXTERN model_loader_run_inference:PROC


; PUBLIC: model_loader_get_metrics() -> rax
;
; Get performance metrics for current session
; - Load time
; - Inference count
; - Total inference time
; - Average inference time
; - Longest inference time
; - Shortest inference time
;
; IN:  (none)
; OUT: rax = pointer to PERF_METRICS structure
;
; PERF_METRICS fields:
;   +0:  load_time_ms           - Model load time
;   +4:  inference_count        - Total inferences
;   +8:  total_inference_time   - Sum of all inference times
;   +16: avg_inference_time_ms  - Average inference time
;   +20: longest_inference_ms   - Longest single inference
;   +24: shortest_inference_ms  - Shortest single inference
;
; Example:
;   call model_loader_get_metrics
;   mov rcx, rax
;   mov eax, [rcx + PERF_METRICS.inference_count]

PUBLIC model_loader_get_metrics
EXTERN model_loader_get_metrics:PROC


; PUBLIC: model_loader_get_state() -> rax
;
; Get current loader state
; - Current model path
; - Model loaded flag
; - Last error message
;
; IN:  (none)
; OUT: rax = pointer to LOADER_STATE structure
;
; LOADER_STATE fields:
;   +0:    initialized           - 1 if initialized
;   +4:    current_model_path    - Model file path
;   +516:  model_loaded          - 1 if loaded
;   +520:  model_arch_string     - Architecture info
;   +1032: last_error            - Error message
;
; Example:
;   call model_loader_get_state
;   mov rcx, rax
;   mov al, [rcx + LOADER_STATE.model_loaded]

PUBLIC model_loader_get_state
EXTERN model_loader_get_state:PROC


; PUBLIC: model_loader_is_initialized() -> eax
;
; Check if loader is initialized
;
; IN:  (none)
; OUT: eax = 1 if initialized, 0 otherwise
;
; Example:
;   call model_loader_is_initialized
;   test eax, eax
;   jz not_initialized

PUBLIC model_loader_is_initialized
EXTERN model_loader_is_initialized:PROC


; PUBLIC: model_loader_unload_current_model() -> eax
;
; Free resources for current model
; - Calls ml_masm_free()
; - Clears loaded flag
;
; IN:  (none)
; OUT: eax = 1 on success
;
; Example:
;   call model_loader_unload_current_model

PUBLIC model_loader_unload_current_model
EXTERN model_loader_unload_current_model:PROC


; PUBLIC: model_loader_run_self_tests() -> (no return)
;
; Execute self-test suite
; - Delegates to test_gguf_loader_main()
;
; IN:  (none)
; OUT: Results printed to debug console
;
; Example:
;   call model_loader_run_self_tests

PUBLIC model_loader_run_self_tests
EXTERN model_loader_run_self_tests:PROC

;=============================================================================
; END API REFERENCE
;=============================================================================
