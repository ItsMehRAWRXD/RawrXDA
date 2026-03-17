// include/masm_lsp_bridge.h
// Pure C ABI for AVX-512 accelerated LSP operations
// Consumed by rawrxd_lsp_bridge.asm

#ifndef MASM_LSP_BRIDGE_H
#define MASM_LSP_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handles
typedef void* rxd_document_t;
typedef void* rxd_cancel_token_t;

// Structure mirrors (packed for MASM compatibility) =========================
#pragma pack(push, 1)

typedef struct {
    uint32_t line;
    uint32_t character;
} rxd_position_t;

typedef struct {
    rxd_position_t start;
    rxd_position_t end;
} rxd_range_t;

typedef struct {
    const char* label;
    uint32_t kind;          // LSP CompletionItemKind
    float score;            // AI confidence
    uint32_t flags;         // Bit 0: Use AVX optimized path
} rxd_completion_item_t;

typedef struct {
    const char* message;
    uint32_t severity;      // 1=Error, 2=Warning, 3=Info, 4=Hint
    rxd_range_t range;
    bool has_fix;
} rxd_diagnostic_t;

#pragma pack(pop)

// Core Bridge Functions (called from C++, implemented in ASM) ================
// All functions return 0 on success, error code on failure

// Text processing
int rxd_asm_normalize_completion(const char* utf8_text, int len);
int rxd_asm_scan_identifiers(const char* buffer, size_t len, void** results, rxd_cancel_token_t ct);
int rxd_asm_calculate_diff(const char* old_buffer, const char* new_buffer, 
                           size_t old_len, size_t new_len,
                           void* diff_buffer, size_t* diff_size);

// Cancellation support
rxd_cancel_token_t rxd_asm_create_cancel_token(void);
void rxd_asm_cancel_token(rxd_cancel_token_t token);
bool rxd_asm_is_cancelled(rxd_cancel_token_t token);
void rxd_asm_destroy_cancel_token(rxd_cancel_token_t token);

// Telemetry/Memory (Zero-copy ring buffer writes)
void rxd_asm_emit_metric(const char* name, int64_t value_ns);
void rxd_asm_copy_to_dma(const char* data, size_t len, uint64_t dma_offset);

// LSP Handoff triggers (called from ASM, implemented in C++)
void rxd_cpp_hover_ready(const char* uri, const char* result_json);
void rxd_cpp_complete_ready(const char* uri, rxd_completion_item_t* items, size_t count);
void rxd_cpp_diagnostic_ready(const char* uri, rxd_diagnostic_t* diags, size_t count);

#ifdef __cplusplus
}
#endif

#endif // MASM_LSP_BRIDGE_H
