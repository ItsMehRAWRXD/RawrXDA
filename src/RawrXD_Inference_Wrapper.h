#pragma once

// RawrXD_Inference_Wrapper.h - Extern "C" linkage for Heretic MASM kernel

// #include <llama.h>  // COMMENTED: Use stubs instead
#include "hotpatch/llama_stub.h"  // Use stub definitions
#include <cstdint>   // int32_t, uint64_t
#include <cstddef>   // size_t

#ifdef __cplusplus
extern "C" {
#endif

// Heretic MASM exports
void Heretic_Main_Entry(llama_token_data_array* candidates);
void Hotpatch_TraceBeacon(const char* msg, uint64_t len);
bool IsUnauthorized_NoDep(int id);

#ifdef __cplusplus
}
#endif