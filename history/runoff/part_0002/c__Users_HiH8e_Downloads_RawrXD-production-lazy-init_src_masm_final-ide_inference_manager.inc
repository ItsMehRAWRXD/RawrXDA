; ============================================================================
; INFERENCE_MANAGER.INC - Model Inference Header
; ============================================================================

EXTERN inference_manager_init:PROC
EXTERN inference_manager_shutdown:PROC
EXTERN load_model:PROC
EXTERN unload_model:PROC
EXTERN tokenize_input:PROC
EXTERN prepare_batch:PROC
EXTERN run_inference:PROC
EXTERN get_logits:PROC
EXTERN sample_token:PROC
EXTERN manage_kv_cache:PROC
EXTERN set_sampling_params:PROC
EXTERN get_inference_stats:PROC
EXTERN cache_embeddings:PROC
EXTERN optimize_memory:PROC

INFERENCE_REQUEST STRUCT
    input_tokens qword
    input_length dword
    max_gen_tokens dword
    output_buffer qword
    request_id dword
    status dword
INFERENCE_REQUEST ENDS

KV_CACHE STRUCT
    key_tensors qword
    value_tensors qword
    seq_position dword
    max_seq_len dword
KV_CACHE ENDS
