/*
 * Entry stub — x64: sub rsp,0x28 (shadow + align); call main; mov ecx,eax; call [IAT ExitProcess]; int3 padding.
 */
#include "entry_stub.h"

/* Corrected stub bytes: 24 bytes so main is at stub_base+24, not overlapping stub. */
const uint8_t entry_stub_bytes[] = {
    0x48, 0x83, 0xEC, 0x28,             /* 0x00: sub rsp, 0x28 (32 shadow + 8 align) */
    0xE8, 0x00, 0x00, 0x00, 0x00,       /* 0x04: call main (rel32 patched) */
    0x89, 0xC1,                         /* 0x09: mov ecx, eax */
    0xFF, 0x15, 0x00, 0x00, 0x00, 0x00, /* 0x0B: call [rip+disp32] (ExitProcess) */
    0xC3, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC  /* 0x11: ret (GCC __main shim), then padding to 24 */
};

const unsigned int entry_stub_size = sizeof(entry_stub_bytes);
