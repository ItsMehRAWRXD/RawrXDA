/*
 * RawrXD PE Generator - Production Header
 * Portable Executable (PE32+) generator for x86-64
 * Pure MASM64 implementation
 */

#ifndef PE_GENERATOR_H
#define PE_GENERATOR_H

#include <stdint.h>
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
   PE GENERATION FUNCTIONS (from RawrXD_PE_Generator_PROD.asm)
   ============================================================================ */

/* 
 * PeGen_Initialize - Initialize context and allocate buffer
 * pContext: 112-byte context buffer (caller allocated)
 * BufferSize: Size of buffer to allocate for PE image
 * Returns: 1 on success, 0 on failure
 */
extern int __fastcall PeGen_Initialize(void* pContext, uint64_t BufferSize);

/* 
 * PeGen_CreateDosHeader - Create MZ/DOS header
 * pContext: Initialized context
 * Returns: 1 on success
 */
extern int __fastcall PeGen_CreateDosHeader(void* pContext);

/* 
 * PeGen_CreateNtHeaders - Create NT/PE32+ headers
 * pContext: Initialized context
 * ImageBase: Desired image base
 * Subsystem: 3 for Console, 2 for GUI
 * Returns: 1 on success
 */
extern int __fastcall PeGen_CreateNtHeaders(void* pContext, uint64_t ImageBase, uint32_t Subsystem);

/* 
 * PeGen_Finalize - Finalize PE structure (Entry point)
 * pContext: Initialized context
 * EntryPointRVA: RVA of the entry point
 * Returns: 1 on success
 */
extern int __fastcall PeGen_Finalize(void* pContext, uint32_t EntryPointRVA);

/* 
 * PeGen_Cleanup - Free context buffer
 * pContext: Initialized context
 */
extern void __fastcall PeGen_Cleanup(void* pContext);

/* ============================================================================
   ENCODER FUNCTIONS
   ============================================================================ */

/* XOR encode data in-place */
extern void __fastcall Encoder_XOR(void* pData, uint64_t nSize, uint64_t qKey);

/* Rotate (ADD+ROR) encode data in-place */
extern void __fastcall Encoder_Rotate(void* pData, uint64_t nSize, uint64_t qKey);

#ifdef __cplusplus
}
#endif

#endif /* PE_GENERATOR_H */
