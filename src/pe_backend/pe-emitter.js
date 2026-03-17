/**
 * PE32+ Machine-Code Backend for RawrXD IDE
 * Production-ready — generates runnable x64 PE executables from IDE compile actions.
 * Zero external dependencies — constructs the entire binary in a Buffer.
 *
 * Architecture:
 *   peImage[0000..01FF]  Headers (DOS + NT + Section Table)
 *   peImage[0200..03FF]  .text   (emitted x64 machine code)
 *   peImage[0400..05FF]  .rdata  (imports + string data)
 *   peImage[0600..07FF]  .data   (writable globals, optional)
 *
 * Companion to pe_emitter.c / pe_emitter.h (C backend) and
 * RawrXD_Monolithic_PE_Emitter.asm (MASM native tool).
 */

'use strict';

const fs = require('fs');
const path = require('path');

// ─── PE Constants ───────────────────────────────────────────────────────────
const FILE_ALIGN = 0x200;
const SECT_ALIGN = 0x1000;
const IMAGE_BASE_LO = 0x40000000;
const IMAGE_BASE_HI = 0x00000001;  // 0x0000000140000000 split for 32-bit writes

const TEXT_RVA = 0x1000;
const RDATA_RVA = 0x2000;
const DATA_RVA = 0x3000;
const TEXT_RAW = 0x0200;
const RDATA_RAW = 0x0400;
const DATA_RAW = 0x0600;

// ─── x64 Register Indices ───────────────────────────────────────────────────
const REG = {
  RAX: 0, RCX: 1, RDX: 2, RBX: 3,
  RSP: 4, RBP: 5, RSI: 6, RDI: 7,
  R8: 8, R9: 9, R10: 10, R11: 11,
  R12: 12, R13: 13, R14: 14, R15: 15
};

/**
 * CodeBuffer — accumulates machine code bytes with offset tracking.
 * Wraps a shared Buffer at a given base offset.
 */
class CodeBuffer {
  constructor(backing, baseOffset) {
    this.buf = backing;
    this.base = baseOffset;
    this.pos = 0;
  }

  get currentRVA() {
    if (this.base === TEXT_RAW) return TEXT_RVA + this.pos;
    if (this.base === RDATA_RAW) return RDATA_RVA + this.pos;
    if (this.base === DATA_RAW) return DATA_RVA + this.pos;
    return this.pos;
  }

  emit(...bytes) {
    for (const b of bytes) {
      this.buf[this.base + this.pos] = b & 0xFF;
      this.pos++;
    }
  }

  emitDword(val) {
    this.buf.writeUInt32LE(val >>> 0, this.base + this.pos);
    this.pos += 4;
  }

  emitQword(lo, hi) {
    this.buf.writeUInt32LE(lo >>> 0, this.base + this.pos);
    this.buf.writeUInt32LE(hi >>> 0, this.base + this.pos + 4);
    this.pos += 8;
  }

  emitString(str) {
    for (let i = 0; i < str.length; i++) {
      this.buf[this.base + this.pos] = str.charCodeAt(i);
      this.pos++;
    }
    this.buf[this.base + this.pos] = 0; // null terminator
    this.pos++;
  }

  emitWord(val) {
    this.buf.writeUInt16LE(val & 0xFFFF, this.base + this.pos);
    this.pos += 2;
  }

  pad(alignment) {
    while (this.pos % alignment !== 0) {
      this.buf[this.base + this.pos] = 0;
      this.pos++;
    }
  }

  get offset() { return this.pos; }
  set offset(v) { this.pos = v; }
}

// ─── x64 Instruction Emitters ───────────────────────────────────────────────

/**
 * Emit x64 instructions into a CodeBuffer.
 * All methods are static — pure functions over the buffer.
 */
class X64Emitter {

  /** sub rsp, imm8 */
  static emitSubRspImm8(cb, imm8) {
    cb.emit(0x48, 0x83, 0xEC, imm8 & 0xFF);
  }

  /** add rsp, imm8 */
  static emitAddRspImm8(cb, imm8) {
    cb.emit(0x48, 0x83, 0xC4, imm8 & 0xFF);
  }

  /** Standard prologue: sub rsp, 28h (shadow space 20h + 8h alignment) */
  static emitPrologue(cb) {
    X64Emitter.emitSubRspImm8(cb, 0x28);
  }

  /** Standard epilogue: add rsp, 28h ; ret — x64 stack frame teardown */
  static emitEpilogue(cb) {
    X64Emitter.emitAddRspImm8(cb, 0x28);
    cb.emit(0xC3); // ret
  }

  /** Frame-pointer prologue: push rbp ; mov rbp, rsp ; sub rsp, N */
  static emitPrologueFP(cb, localSize) {
    cb.emit(0x55);                          // push rbp
    cb.emit(0x48, 0x89, 0xE5);              // mov rbp, rsp
    X64Emitter.emitSubRspImm8(cb, localSize);
  }

  /** Frame-pointer epilogue: mov rsp, rbp ; pop rbp ; ret */
  static emitEpilogueFP(cb) {
    cb.emit(0x48, 0x89, 0xEC);              // mov rsp, rbp
    cb.emit(0x5D);                          // pop rbp
    cb.emit(0xC3);                          // ret
  }

  /** xor r32, r32 — zero a register (regs 0-7) */
  static emitXorReg32(cb, reg) {
    const modrm = 0xC0 | (reg << 3) | reg;
    cb.emit(0x33, modrm);
  }

  /** xor r32, r32 for r8d-r15d (REX.RB prefix) */
  static emitXorReg32High(cb, reg) {
    const r = reg - 8;
    const modrm = 0xC0 | (r << 3) | r;
    cb.emit(0x45, 0x33, modrm);
  }

  /** lea reg, [rip+disp32] — reg 0-7 (REX.W) or 8-15 (REX.WR) */
  static emitLeaRipReg(cb, reg, disp32) {
    if (reg < 8) {
      cb.emit(0x48, 0x8D, 0x05 | (reg << 3));
    } else {
      cb.emit(0x4C, 0x8D, 0x05 | ((reg - 8) << 3));
    }
    cb.emitDword(disp32);
  }

  /** call [rip+disp32] — indirect call through IAT slot */
  static emitCallRipIndirect(cb, disp32) {
    cb.emit(0xFF, 0x15);
    cb.emitDword(disp32);
  }

  /** mov ecx, imm32 */
  static emitMovEcxImm32(cb, imm32) {
    cb.emit(0xB9);
    cb.emitDword(imm32);
  }

  /** ret */
  static emitRet(cb) {
    cb.emit(0xC3);
  }

  /** nop */
  static emitNop(cb) {
    cb.emit(0x90);
  }

  /** int3 (breakpoint) */
  static emitInt3(cb) {
    cb.emit(0xCC);
  }

  /** push r64 (0-15) */
  static emitPushReg(cb, reg) {
    if (reg < 8) {
      cb.emit(0x50 + reg);
    } else {
      cb.emit(0x41, 0x50 + (reg - 8));
    }
  }

  /** pop r64 (0-15) */
  static emitPopReg(cb, reg) {
    if (reg < 8) {
      cb.emit(0x58 + reg);
    } else {
      cb.emit(0x41, 0x58 + (reg - 8));
    }
  }

  /** mov r64, imm64 (movabs — 10-byte encoding) */
  static emitMovRegImm64(cb, reg, lo, hi) {
    if (reg < 8) {
      cb.emit(0x48, 0xB8 + reg);
    } else {
      cb.emit(0x49, 0xB8 + (reg - 8));
    }
    cb.emitQword(lo, hi);
  }

  /** mov r64, r64 */
  static emitMovR64R64(cb, dst, src) {
    let rex = 0x48;
    let dstEnc = dst;
    let srcEnc = src;
    if (src >= 8) { rex |= 0x04; srcEnc -= 8; }
    if (dst >= 8) { rex |= 0x01; dstEnc -= 8; }
    cb.emit(rex, 0x89, 0xC0 | (srcEnc << 3) | dstEnc);
  }

  /** mov r32, imm32 (5-byte encoding for regs 0-7, 6-byte for 8-15) */
  static emitMovR32Imm32(cb, reg, imm32) {
    if (reg < 8) {
      cb.emit(0xB8 + reg);
    } else {
      cb.emit(0x41, 0xB8 + (reg - 8));
    }
    cb.emitDword(imm32);
  }

  /** add r64, imm32 (signed) */
  static emitAddR64Imm32(cb, reg, imm32) {
    let rex = 0x48;
    let rm = reg;
    if (reg >= 8) { rex |= 0x01; rm -= 8; }
    cb.emit(rex, 0x81, 0xC0 | rm);
    cb.emitDword(imm32);
  }

  /** sub r64, imm32 (signed) */
  static emitSubR64Imm32(cb, reg, imm32) {
    let rex = 0x48;
    let rm = reg;
    if (reg >= 8) { rex |= 0x01; rm -= 8; }
    cb.emit(rex, 0x81, 0xE8 | rm);
    cb.emitDword(imm32);
  }

  /** cmp r64, imm32 (signed) */
  static emitCmpR64Imm32(cb, reg, imm32) {
    let rex = 0x48;
    let rm = reg;
    if (reg >= 8) { rex |= 0x01; rm -= 8; }
    cb.emit(rex, 0x81, 0xF8 | rm);
    cb.emitDword(imm32);
  }

  /** test r64, r64 */
  static emitTestR64R64(cb, dst, src) {
    let rex = 0x48;
    let dstEnc = dst;
    let srcEnc = src;
    if (src >= 8) { rex |= 0x04; srcEnc -= 8; }
    if (dst >= 8) { rex |= 0x01; dstEnc -= 8; }
    cb.emit(rex, 0x85, 0xC0 | (srcEnc << 3) | dstEnc);
  }

  /** call reg (2 bytes for 0-7, 3 bytes for 8-15) */
  static emitCallReg(cb, reg) {
    if (reg >= 8) cb.emit(0x41);
    cb.emit(0xFF, 0xD0 | (reg & 7));
  }

  /** jmp reg */
  static emitJmpReg(cb, reg) {
    if (reg >= 8) cb.emit(0x41);
    cb.emit(0xFF, 0xE0 | (reg & 7));
  }

  /** syscall */
  static emitSyscall(cb) {
    cb.emit(0x0F, 0x05);
  }

  /** NOP sled */
  static emitNopSled(cb, count) {
    for (let i = 0; i < count; i++) cb.emit(0x90);
  }

  /**
   * Calculate RIP-relative displacement.
   * @param {number} instrEndRVA — RVA of byte AFTER the instruction
   * @param {number} targetRVA   — RVA of target
   * @returns {number} signed 32-bit displacement
   */
  static ripDisp(instrEndRVA, targetRVA) {
    return (targetRVA - instrEndRVA) | 0;
  }
}

// ─── PE Image Builder ───────────────────────────────────────────────────────

class PEBuilder {
  constructor(options = {}) {
    this.numSections = options.numSections || 3;
    this.totalSize = FILE_ALIGN * (1 + this.numSections);
    this.image = Buffer.alloc(this.totalSize, 0);
    this.subsystem = options.subsystem || 0x0003; // CUI default
    this.entryRVA = options.entryRVA || TEXT_RVA;

    this.imports = [];
    this.iatSlots = new Map();
    this.stringMap = new Map();

    this.code = new CodeBuffer(this.image, TEXT_RAW);
    this.rdata = new CodeBuffer(this.image, RDATA_RAW);
    this.data = new CodeBuffer(this.image, DATA_RAW);
  }

  /**
   * Register a DLL import.
   * @param {string} dll
   * @param {string[]} functions
   */
  addImport(dll, functions) {
    this.imports.push({ dll, functions });
  }

  /**
   * Add string to .rdata. Must call AFTER buildImports().
   * @param {string} text
   * @returns {number} RVA of the string
   */
  addString(text) {
    if (this.stringMap.has(text)) return this.stringMap.get(text);
    const rva = RDATA_RVA + this.rdata.offset;
    this.rdata.emitString(text);
    this.rdata.pad(2);
    this.stringMap.set(text, rva);
    return rva;
  }

  // ─── Header Emission ────────────────────────────────────────────────

  buildDOSHeader() {
    const b = this.image;
    b.writeUInt16LE(0x5A4D, 0x00);      // e_magic = "MZ"
    b.writeUInt32LE(0x00000080, 0x3C);   // e_lfanew
  }

  buildDOSStub() {
    // Minimal: CD 21 C3 (INT 21h, RET)
    this.image[0x40] = 0xCD;
    this.image[0x41] = 0x21;
    this.image[0x42] = 0xC3;
  }

  buildNTHeaders() {
    const b = this.image;
    const sizeOfImage = SECT_ALIGN * (1 + this.numSections);

    // PE Signature
    b.writeUInt32LE(0x00004550, 0x80);

    // COFF File Header (20 bytes @ 0x84)
    b.writeUInt16LE(0x8664, 0x84);       // Machine = AMD64
    b.writeUInt16LE(this.numSections, 0x86);
    b.writeUInt32LE(0, 0x88);            // TimeDateStamp
    b.writeUInt32LE(0, 0x8C);
    b.writeUInt32LE(0, 0x90);
    b.writeUInt16LE(0x00F0, 0x94);       // SizeOfOptionalHeader = 240
    b.writeUInt16LE(0x0022, 0x96);       // EXEC | LARGE_ADDR

    // Optional Header PE32+ (240 bytes @ 0x98)
    b.writeUInt16LE(0x020B, 0x98);       // Magic = PE32+
    b[0x9A] = 14;                        // MajorLinkerVersion
    b[0x9B] = 0;
    b.writeUInt32LE(FILE_ALIGN, 0x9C);   // SizeOfCode
    b.writeUInt32LE(FILE_ALIGN * 2, 0xA0);
    b.writeUInt32LE(0, 0xA4);
    b.writeUInt32LE(this.entryRVA, 0xA8);
    b.writeUInt32LE(TEXT_RVA, 0xAC);
    b.writeUInt32LE(IMAGE_BASE_LO, 0xB0);
    b.writeUInt32LE(IMAGE_BASE_HI, 0xB4);
    b.writeUInt32LE(SECT_ALIGN, 0xB8);
    b.writeUInt32LE(FILE_ALIGN, 0xBC);
    b.writeUInt16LE(6, 0xC0);
    b.writeUInt16LE(0, 0xC2);
    b.writeUInt16LE(0, 0xC4);
    b.writeUInt16LE(0, 0xC6);
    b.writeUInt16LE(6, 0xC8);
    b.writeUInt16LE(0, 0xCA);
    b.writeUInt32LE(0, 0xCC);
    b.writeUInt32LE(sizeOfImage, 0xD0);
    b.writeUInt32LE(FILE_ALIGN, 0xD4);
    b.writeUInt32LE(0, 0xD8);
    b.writeUInt16LE(this.subsystem, 0xDC);
    b.writeUInt16LE(0x8160, 0xDE);       // DYNAMIC_BASE|NX_COMPAT|TS_AWARE|HI_ENTROPY
    b.writeUInt32LE(0x00100000, 0xE0); b.writeUInt32LE(0, 0xE4);
    b.writeUInt32LE(0x00001000, 0xE8); b.writeUInt32LE(0, 0xEC);
    b.writeUInt32LE(0x00100000, 0xF0); b.writeUInt32LE(0, 0xF4);
    b.writeUInt32LE(0x00001000, 0xF8); b.writeUInt32LE(0, 0xFC);
    b.writeUInt32LE(0, 0x100);
    b.writeUInt32LE(16, 0x104);          // NumberOfRvaAndSizes
  }

  buildSectionHeaders() {
    const b = this.image;
    let off = 0x188;

    // .text
    this._writeSectionName(off, '.text');
    b.writeUInt32LE(FILE_ALIGN, off + 0x08);
    b.writeUInt32LE(TEXT_RVA, off + 0x0C);
    b.writeUInt32LE(FILE_ALIGN, off + 0x10);
    b.writeUInt32LE(TEXT_RAW, off + 0x14);
    b.writeUInt32LE(0x60000020, off + 0x24);   // CODE|EXECUTE|READ

    // .rdata
    off += 40;
    this._writeSectionName(off, '.rdata');
    b.writeUInt32LE(FILE_ALIGN, off + 0x08);
    b.writeUInt32LE(RDATA_RVA, off + 0x0C);
    b.writeUInt32LE(FILE_ALIGN, off + 0x10);
    b.writeUInt32LE(RDATA_RAW, off + 0x14);
    b.writeUInt32LE(0x40000040, off + 0x24);   // IDATA|READ

    if (this.numSections >= 3) {
      off += 40;
      this._writeSectionName(off, '.data');
      b.writeUInt32LE(FILE_ALIGN, off + 0x08);
      b.writeUInt32LE(DATA_RVA, off + 0x0C);
      b.writeUInt32LE(FILE_ALIGN, off + 0x10);
      b.writeUInt32LE(DATA_RAW, off + 0x14);
      b.writeUInt32LE(0xC0000040, off + 0x24);  // IDATA|READ|WRITE
    }
  }

  _writeSectionName(offset, name) {
    for (let i = 0; i < 8; i++) {
      this.image[offset + i] = i < name.length ? name.charCodeAt(i) : 0;
    }
  }

  // ─── Import Table Builder ───────────────────────────────────────────

  /**
   * Build Import Directory Table, ILT, IAT, Hint/Name, DLL name strings.
   * Must be called AFTER addImport() and BEFORE emitting code that
   * references IAT slots.
   */
  buildImports() {
    const rd = this.rdata;
    const numDlls = this.imports.length;
    const idtSize = (numDlls + 1) * 20;

    let totalFuncs = 0;
    for (const imp of this.imports) totalFuncs += imp.functions.length;

    // Phase 1: Calculate offsets
    let iltStart = idtSize;
    let iatStart = iltStart;
    for (const imp of this.imports) {
      iatStart += (imp.functions.length + 1) * 8;
    }

    let hnStart = iatStart;
    for (const imp of this.imports) {
      hnStart += (imp.functions.length + 1) * 8;
    }

    // Phase 2: Compute Hint/Name RVAs
    const hnEntries = [];
    let hnCursor = hnStart;
    for (const imp of this.imports) {
      for (const fn of imp.functions) {
        const rva = RDATA_RVA + hnCursor;
        hnEntries.push({ name: fn, rva, rdataOffset: hnCursor });
        hnCursor += 2 + fn.length + 1;
        if (hnCursor % 2 !== 0) hnCursor++;
      }
    }

    // DLL name strings
    let dllNameCursor = hnCursor;
    const dllNameRVAs = [];
    for (const imp of this.imports) {
      dllNameRVAs.push(RDATA_RVA + dllNameCursor);
      dllNameCursor += imp.dll.length + 1;
    }

    // Phase 3: Write IDT entries
    rd.offset = 0;
    let iltCur = iltStart;
    let iatCur = iatStart;
    let hnIdx = 0;

    for (let i = 0; i < numDlls; i++) {
      const imp = this.imports[i];
      rd.emitDword(RDATA_RVA + iltCur);     // OriginalFirstThunk
      rd.emitDword(0);                       // TimeDateStamp
      rd.emitDword(0);                       // ForwarderChain
      rd.emitDword(dllNameRVAs[i]);          // Name RVA
      rd.emitDword(RDATA_RVA + iatCur);     // FirstThunk

      iltCur += (imp.functions.length + 1) * 8;
      iatCur += (imp.functions.length + 1) * 8;
    }
    // Null terminator IDT entry
    for (let i = 0; i < 5; i++) rd.emitDword(0);

    // Phase 4: Write ILT entries
    rd.offset = iltStart;
    hnIdx = 0;
    for (const imp of this.imports) {
      for (const fn of imp.functions) {
        const hn = hnEntries[hnIdx++];
        rd.emitQword(hn.rva, 0);
      }
      rd.emitQword(0, 0);
    }

    // Phase 5: Write IAT entries (same as ILT — loader overwrites at load)
    rd.offset = iatStart;
    hnIdx = 0;
    let iatSlotOffset = iatStart;
    for (const imp of this.imports) {
      for (const fn of imp.functions) {
        const hn = hnEntries[hnIdx++];
        this.iatSlots.set(fn, RDATA_RVA + iatSlotOffset);
        rd.emitQword(hn.rva, 0);
        iatSlotOffset += 8;
      }
      rd.emitQword(0, 0);
      iatSlotOffset += 8;
    }

    // Phase 6: Write Hint/Name entries
    for (const hn of hnEntries) {
      rd.offset = hn.rdataOffset;
      rd.emitWord(0);
      rd.emitString(hn.name);
      rd.pad(2);
    }

    // Phase 7: Write DLL name strings
    rd.offset = dllNameCursor - (this.imports.reduce((a, i) => a + i.dll.length + 1, 0));
    for (const imp of this.imports) {
      rd.emitString(imp.dll);
    }

    rd.offset = dllNameCursor;

    // Phase 8: Fill data directory entries in NT headers
    const b = this.image;
    b.writeUInt32LE(RDATA_RVA, 0x110);                    // [1] Import Dir RVA
    b.writeUInt32LE(idtSize, 0x114);                       // [1] Import Dir Size
    b.writeUInt32LE(RDATA_RVA + iatStart, 0x168);          // [12] IAT RVA
    b.writeUInt32LE((totalFuncs + numDlls) * 8, 0x16C);   // [12] IAT Size
  }

  /**
   * Get the IAT RVA for a given function name.
   * @param {string} functionName
   * @returns {number} RVA of the IAT slot
   * @throws if import not registered
   */
  getIATSlot(functionName) {
    const rva = this.iatSlots.get(functionName);
    if (rva === undefined) {
      throw new Error(`Import not found: ${functionName}`);
    }
    return rva;
  }

  // ─── Full Build Orchestration ───────────────────────────────────────

  /**
   * Build headers + imports. Call after addImport() setup.
   * @returns {Buffer} The image buffer (code can still be emitted after)
   */
  build() {
    this.buildDOSHeader();
    this.buildDOSStub();
    this.buildNTHeaders();
    this.buildSectionHeaders();
    this.buildImports();
    return this.image;
  }

  /**
   * Write the PE image to disk.
   * @param {string} outputPath
   */
  writeToDisk(outputPath) {
    fs.writeFileSync(outputPath, this.image);
  }
}

// ─── High-Level Compilation API ─────────────────────────────────────────────

/**
 * Compile a GUI program (MessageBox + ExitProcess) to a PE32+ executable.
 *
 * @param {Object} program
 * @param {string} program.title
 * @param {string} program.message
 * @param {number} program.exitCode
 * @param {string} program.subsystem — 'console' or 'gui'
 * @param {string} outputPath
 * @returns {string} outputPath
 */
function compileProgram(program, outputPath) {
  const pe = new PEBuilder({
    subsystem: program.subsystem === 'gui' ? 0x0002 : 0x0003,
    numSections: 3
  });

  pe.addImport('user32.dll', ['MessageBoxA']);
  pe.addImport('kernel32.dll', ['ExitProcess']);

  pe.buildDOSHeader();
  pe.buildDOSStub();
  pe.buildNTHeaders();
  pe.buildSectionHeaders();
  pe.buildImports();

  const titleRVA = pe.addString(program.title || 'RawrXD');
  const msgRVA = pe.addString(program.message || 'Hello from RawrXD IDE!');

  const iatMsgBox = pe.getIATSlot('MessageBoxA');
  const iatExit = pe.getIATSlot('ExitProcess');

  const cb = pe.code;

  // sub rsp, 28h
  X64Emitter.emitPrologue(cb);

  // xor ecx, ecx  (hWnd = NULL)
  X64Emitter.emitXorReg32(cb, REG.RCX);

  // lea rdx, [rip+disp] -> message
  const leaRdxEndRVA = TEXT_RVA + cb.offset + 7;
  X64Emitter.emitLeaRipReg(cb, REG.RDX, X64Emitter.ripDisp(leaRdxEndRVA, msgRVA));

  // lea r8, [rip+disp] -> title
  const leaR8EndRVA = TEXT_RVA + cb.offset + 7;
  X64Emitter.emitLeaRipReg(cb, REG.R8, X64Emitter.ripDisp(leaR8EndRVA, titleRVA));

  // xor r9d, r9d  (uType = MB_OK)
  X64Emitter.emitXorReg32High(cb, REG.R9);

  // call [rip+disp] -> MessageBoxA
  const callMbEndRVA = TEXT_RVA + cb.offset + 6;
  X64Emitter.emitCallRipIndirect(cb, X64Emitter.ripDisp(callMbEndRVA, iatMsgBox));

  // xor ecx, ecx  (exit code)
  if (program.exitCode && program.exitCode !== 0) {
    X64Emitter.emitMovEcxImm32(cb, program.exitCode);
  } else {
    X64Emitter.emitXorReg32(cb, REG.RCX);
  }

  // call [rip+disp] -> ExitProcess
  const callExEndRVA = TEXT_RVA + cb.offset + 6;
  X64Emitter.emitCallRipIndirect(cb, X64Emitter.ripDisp(callExEndRVA, iatExit));

  // Epilogue (structurally correct, unreachable after ExitProcess)
  X64Emitter.emitEpilogue(cb);

  pe.writeToDisk(outputPath);
  return outputPath;
}

/**
 * Compile a console program (WriteFile to stdout + ExitProcess).
 *
 * @param {Object} program
 * @param {string} program.message
 * @param {string} outputPath
 * @returns {string} outputPath
 */
function compileConsoleProgram(program, outputPath) {
  const pe = new PEBuilder({
    subsystem: 0x0003,
    numSections: 3
  });

  pe.addImport('kernel32.dll', [
    'GetStdHandle', 'WriteFile', 'ExitProcess'
  ]);

  pe.buildDOSHeader();
  pe.buildDOSStub();
  pe.buildNTHeaders();
  pe.buildSectionHeaders();
  pe.buildImports();

  const messageText = program.message || 'Hello from RawrXD IDE!\r\n';
  const msgRVA = pe.addString(messageText);
  const msgLen = messageText.length;

  // Reserve 4 bytes in .data for bytesWritten
  const bytesWrittenRVA = DATA_RVA + pe.data.offset;
  pe.data.emitDword(0);

  const iatGetStdHandle = pe.getIATSlot('GetStdHandle');
  const iatWriteFile = pe.getIATSlot('WriteFile');
  const iatExitProcess = pe.getIATSlot('ExitProcess');

  const cb = pe.code;

  // sub rsp, 38h (extra shadow + 5th param on stack)
  X64Emitter.emitSubRspImm8(cb, 0x38);

  // mov ecx, -11 (STD_OUTPUT_HANDLE)
  cb.emit(0xB9);
  cb.emitDword(0xFFFFFFF5);

  // call GetStdHandle
  const callGshEnd = TEXT_RVA + cb.offset + 6;
  X64Emitter.emitCallRipIndirect(cb, X64Emitter.ripDisp(callGshEnd, iatGetStdHandle));

  // mov rcx, rax  (handle)
  cb.emit(0x48, 0x89, 0xC1);

  // lea rdx, [rip+disp] -> message
  const leaRdxEnd = TEXT_RVA + cb.offset + 7;
  X64Emitter.emitLeaRipReg(cb, REG.RDX, X64Emitter.ripDisp(leaRdxEnd, msgRVA));

  // mov r8d, msgLen
  cb.emit(0x41, 0xB8);
  cb.emitDword(msgLen);

  // lea r9, [rip+disp] -> bytesWritten
  const leaR9End = TEXT_RVA + cb.offset + 7;
  X64Emitter.emitLeaRipReg(cb, REG.R9, X64Emitter.ripDisp(leaR9End, bytesWrittenRVA));

  // mov QWORD PTR [rsp+20h], 0  (lpOverlapped = NULL)
  cb.emit(0x48, 0xC7, 0x44, 0x24, 0x20);
  cb.emitDword(0);

  // call WriteFile
  const callWfEnd = TEXT_RVA + cb.offset + 6;
  X64Emitter.emitCallRipIndirect(cb, X64Emitter.ripDisp(callWfEnd, iatWriteFile));

  // xor ecx, ecx
  X64Emitter.emitXorReg32(cb, REG.RCX);

  // call ExitProcess
  const callEpEnd = TEXT_RVA + cb.offset + 6;
  X64Emitter.emitCallRipIndirect(cb, X64Emitter.ripDisp(callEpEnd, iatExitProcess));

  // Epilogue
  X64Emitter.emitAddRspImm8(cb, 0x38);
  X64Emitter.emitRet(cb);

  pe.writeToDisk(outputPath);
  return outputPath;
}

// ─── Exports ────────────────────────────────────────────────────────────────
module.exports = {
  PEBuilder,
  X64Emitter,
  CodeBuffer,
  REG,
  compileProgram,
  compileConsoleProgram,
  // Constants
  TEXT_RVA, RDATA_RVA, DATA_RVA,
  IMAGE_BASE_LO, IMAGE_BASE_HI,
  FILE_ALIGN, SECT_ALIGN
};
