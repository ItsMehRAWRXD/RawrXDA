/*
 * Entry stub — minimal C runtime: call main, then ExitProcess(EAX).
 * x64: RSP 16-byte aligned before CALL; 32-byte shadow space.
 * Stub is generated in C; rel32 to main and disp32 to IAT are patched by linker.
 */
#ifndef PHASE2_ENTRY_STUB_H
#define PHASE2_ENTRY_STUB_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Stub size. Must match entry_stub.c. Object code starts at stub_base + ENTRY_STUB_SIZE. */
#define ENTRY_STUB_SIZE 24u

/* Offsets within stub (from entry point). */
#define ENTRY_STUB_REL32_MAIN_OFFSET      5u   /* rel32 in 'call main' */
#define ENTRY_STUB_DISP32_IAT_OFFSET     13u   /* disp32 in 'call [rip+disp32]' */
#define ENTRY_STUB_CALL_IAT_NEXT_RIP_OFFSET 17u  /* RIP after call [rip+disp32] */
/* Offset of single 'ret' (0xC3) for GCC __main shim — do not resolve __main to main. */
#define ENTRY_STUB_RET_OFFSET 17u

extern const uint8_t entry_stub_bytes[];
extern const unsigned int entry_stub_size;

#ifdef __cplusplus
}
#endif

#endif /* PHASE2_ENTRY_STUB_H */
