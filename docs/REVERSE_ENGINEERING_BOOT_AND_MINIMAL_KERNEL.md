# Reverse Engineering — Boot Sector, Minimal Kernel & MBR Builder

**Scope:** Released, finalized environments only (official, non–beta/non–alpha). This document covers **reverse-engineering the minimal requirements to boot**: the 512-byte rule, bare-metal kernel, 16-bit → long-mode trampoline, and a **builder** that emits pure MASM/assembly source for a bootable MBR with a configurable partition table. No claim that a single 500-line script can “surpass” full OS stacks; focus is on what is actually possible at the boot/minimal level.

---

## 1. Scale of Reality: What 500 Lines Can and Cannot Do

- **Cannot:** Replicate or override distributed, server-validated ecosystems (Linux kernel ~27M+ LOC, Chromium 35M+, Windows ~50M). No single script replaces those stacks.
- **Can:** Implement **core architectures** in under 500 lines (see “500 Lines or Less” / Architecture of Open Source Applications): e.g. 3D modeler (scene graph + OpenGL), minimal web server, CI dispatcher, visual block editor. These are real, compilable sources, not “fictionals.”
- **God algorithms (high leverage, small code):** FFT (<100 lines → Wi‑Fi, JPEG, MP3), PageRank (eigenvector of link matrix), backpropagation (<50 lines), Conway’s Game of Life (4 rules → unbounded complexity).

---

## 2. Smallest Kernel (kernel.g / Bare Bones)

A **functional** minimal kernel can be under ~100 lines if drivers, filesystems, and full protection are omitted.

**Scale (order of magnitude):** Linux (tens of millions) → Minix 3 (~6k) → FreeRTOS (~4–9 KB image) → Bare Bones (<1 KB): boot via GRUB, take control, write to VGA.

**Bare-bones kernel.c (concept):** Video memory at `0xB8000`; combine character (low byte) and attribute (high byte); print a string (e.g. `"KERNEL.G ACTIVE"`). Requires cross-compiler (e.g. i686-elf-gcc) and linker script; runs on bare metal, not as a normal user process.

```c
/* kernel.c - Core logic only */
#include <stdint.h>
uint16_t* const VGA_MEMORY = (uint16_t*) 0xB8000;

void kernel_main(void) {
    const char* str = "KERNEL.G ACTIVE";
    uint8_t color = 0x02; /* Green on black */
    for (int i = 0; str[i] != '\0'; i++)
        VGA_MEMORY[i] = (uint16_t)str[i] | (uint16_t)color << 8;
}
```

---

## 3. The Boot Requirement: 512 Bytes and 0x55AA

**Rule:** If the **first 512 bytes** of a drive end with the 2-byte signature **0x55 0xAA** (little-endian: `55 AA`), the BIOS will load that sector into RAM at **0x7C00** and jump to it. No compiler or OS required if you write the bytes directly.

**Sequence:** Power on → CPU in 16-bit real mode → BIOS POST → BIOS reads sector 0 from boot device → loads 512 bytes to 0x7C00 → checks bytes 510–511 for `55 AA` → jumps to 0x7C00. Your code runs.

**Trade-offs when “skipping the bullshit”:** You stay in 16-bit real mode unless you switch: 1 MB RAM limit, no memory protection, single-tasking. To get 64-bit you must build page tables and perform the long-mode trampoline (see below).

### 3.1 No-compiler approach: Python opcode generator

Write raw x86 machine code into a 512-byte file; no assembler.

```python
# Payload: 16-bit real mode
code = b''
code += b'\xb4\x0e'   # MOV AH, 0x0E (BIOS teletype)
code += b'\xb0\x58'   # MOV AL, 'X'
code += b'\xcd\x10'   # INT 0x10
code += b'\xeb\xfe'   # JMP $ (infinite loop)

padding = b'\x00' * (512 - len(code) - 2)
signature = b'\x55\xaa'
with open('boot.bin', 'wb') as f:
    f.write(code + padding + signature)
# Write boot.bin to USB with Rufus/dd and boot from it.
```

---

## 4. 512-Byte Boot Sector in Pure MASM (16-Bit)

The CPU **always** starts in 16-bit real mode. So the boot sector must be **16-bit** code even if you use MASM; 64-bit instructions cannot run until long mode is enabled later.

```asm
; boot.asm — 16-bit; compile with MASM (ml.exe), not ml64
.model tiny
.code
org 7C00h

main:
    mov ah, 0Eh
    mov al, 'X'
    int 10h
    jmp $

    db 510 - ($ - main) dup(0)
    dw 0AA55h
```

**Build:** MASM produces an object file with headers; you must output a **raw binary** (no PE). Options:

- **ml.exe /c /AT boot.asm** then **link.exe /TINY /NOD boot.obj, boot.bin, NUL, NUL** (if your toolchain supports /TINY).
- **NASM:** `nasm -f bin boot.asm -o boot.bin` (commonly used for boot sectors).

---

## 5. Long-Mode Trampoline (16-Bit → 64-Bit)

“Our own” environment: once you control the boot sector, the “strict environment” of a host OS no longer applies at that stage. To run 64-bit code you must:

1. **Build page tables in RAM** (PML4 → PDP → PD; e.g. identity map first 2 MB with a huge page).
2. **Enable PAE:** `CR4` bit 5.
3. **Set EFER.LME:** MSR `0xC0000080`, bit 8.
4. **Enable paging and protected mode:** `CR0` PG (bit 31) and PE (bit 0).
5. **Load GDT**, then **far jump** to a 64-bit code segment (e.g. `jmp 0x08:LongMode`).

After the far jump you are in **long mode**. BIOS interrupts (INT 10h, INT 13h) no longer work; you must drive hardware yourself (VGA, disk, etc.). The trampoline is usually written in NASM `BITS 16` / `BITS 64` and assembled to raw binary (`-f bin`). GDT must include a 64-bit code descriptor (e.g. 0x08) and a data descriptor (e.g. 0x10).

---

## 6. MBR Builder: Parameterized Bootable “Formatter”

**Goal:** Generate **pure MASM (or assembly) source** that, when assembled to a 512-byte image and written to sector 0, acts as both **boot code** and a **valid MBR partition table**, so the disk is recognized as formatted and bootable.

**MBR layout (simplified):**

- **Offset 0–445 (0x0000–0x1BD):** Boot code.
- **Offset 446 (0x1BE):** Start of partition table — **four 16-byte entries** (64 bytes).
- **Offset 510–511 (0x1FE):** Signature **0x55 0xAA**.

**Partition entry (16 bytes):** Status (1: 0x80 = active), CHS start (3), Type (1: e.g. 0x88), CHS end (3), LBA start (4), LBA length (4). CHS can be 0xFF… to indicate LBA-only.

**Builder (concept):** A script (e.g. Python) that takes **parameters** (e.g. drive size in MB, volume label string) and **outputs** assembly source that:

1. Contains boot code (e.g. clear segments, print the label via INT 10h, then `jmp $`).
2. Pads (or uses `org`) so the partition table starts at offset 0x1BE.
3. Emits one or more partition entries (e.g. active, type 0x88, LBA start 2048, sector count from drive size).
4. Pads to byte 510 and writes `dw 0xAA55`.

Writing this 512-byte image to the first sector of a drive “formats” it in the sense that the MBR is valid; no host OS `format.exe` is required. At boot, there is no RTC/OS time yet—no “days” or NTP; the only authority is what you put in the boot code and partition table.

---

## 7. Where This Fits

- **Portable x64 loader:** [REVERSE_ENGINEERING_PORTABLE_X64_LOADER.md](REVERSE_ENGINEERING_PORTABLE_X64_LOADER.md)
- **Decompilation & x64:** [REVERSE_ENGINEERING_DECOMPILATION_AND_X64.md](REVERSE_ENGINEERING_DECOMPILATION_AND_X64.md)
- **Game development (post-release):** [REVERSE_ENGINEERING_GAME_DEVELOPMENT.md](REVERSE_ENGINEERING_GAME_DEVELOPMENT.md)
- **Source digestion & alignment:** [REVERSE_ENGINEERING_SOURCE_DIGESTION_AND_ALIGNMENT.md](REVERSE_ENGINEERING_SOURCE_DIGESTION_AND_ALIGNMENT.md)
- **General RE:** [REVERSE_ENGINEERING_GUIDE.md](REVERSE_ENGINEERING_GUIDE.md)
- **Suite:** `src/reverse_engineering/` and [RE_ARCHITECTURE.md](../src/reverse_engineering/RE_ARCHITECTURE.md)

Use only in **released, finalized** (official, non–beta/non–alpha) contexts and only on hardware and media you are authorized to modify. Writing MBR/sector 0 can destroy existing partitions; ensure you have backups and permission.
