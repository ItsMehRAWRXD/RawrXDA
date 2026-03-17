/**
 * PE32+ Backend — Full Test Suite
 *
 * Tests:
 *   1. CodeBuffer byte emission, dword/qword/string, position tracking
 *   2. X64Emitter — prologue/epilogue/FP variants, xor, lea, call, push/pop
 *   3. PEBuilder — DOS header, PE sig, COFF, optional header, section headers
 *   4. Import table — IAT slot resolution, multi-DLL registration
 *   5. End-to-end — compileProgram() and compileConsoleProgram() produce valid PEs
 *
 * Run:  node --test test/pe-emitter.test.js
 *       (from d:\rawrxd)
 */

'use strict';

const { describe, it } = require('node:test');
const assert = require('node:assert');
const fs = require('fs');
const path = require('path');

const {
  PEBuilder, X64Emitter, CodeBuffer, REG,
  compileProgram, compileConsoleProgram,
  TEXT_RVA, RDATA_RVA, DATA_RVA,
  FILE_ALIGN, SECT_ALIGN
} = require('../src/pe_backend/pe-emitter');

// ─── CodeBuffer ─────────────────────────────────────────────────────────────

describe('CodeBuffer', () => {
  it('should emit bytes and track position', () => {
    const buf = Buffer.alloc(64, 0);
    const cb = new CodeBuffer(buf, 0);
    cb.emit(0x90, 0x90, 0x90);
    assert.strictEqual(cb.offset, 3);
    assert.deepStrictEqual([...buf.subarray(0, 3)], [0x90, 0x90, 0x90]);
  });

  it('should emit at a base offset', () => {
    const buf = Buffer.alloc(64, 0);
    const cb = new CodeBuffer(buf, 10);
    cb.emit(0xCC);
    assert.strictEqual(buf[10], 0xCC);
    assert.strictEqual(buf[0], 0);
  });

  it('should emit strings with null terminator', () => {
    const buf = Buffer.alloc(64, 0);
    const cb = new CodeBuffer(buf, 0);
    cb.emitString('ABC');
    assert.strictEqual(buf[0], 65); // 'A'
    assert.strictEqual(buf[1], 66); // 'B'
    assert.strictEqual(buf[2], 67); // 'C'
    assert.strictEqual(buf[3], 0);
    assert.strictEqual(cb.offset, 4);
  });

  it('should write dwords in little-endian', () => {
    const buf = Buffer.alloc(64, 0);
    const cb = new CodeBuffer(buf, 0);
    cb.emitDword(0x12345678);
    assert.strictEqual(buf.readUInt32LE(0), 0x12345678);
    assert.strictEqual(cb.offset, 4);
  });

  it('should write qwords (lo+hi) in little-endian', () => {
    const buf = Buffer.alloc(64, 0);
    const cb = new CodeBuffer(buf, 0);
    cb.emitQword(0x40000000, 0x00000001);
    assert.strictEqual(buf.readUInt32LE(0), 0x40000000);
    assert.strictEqual(buf.readUInt32LE(4), 0x00000001);
    assert.strictEqual(cb.offset, 8);
  });

  it('should write words in little-endian', () => {
    const buf = Buffer.alloc(64, 0);
    const cb = new CodeBuffer(buf, 0);
    cb.emitWord(0x5A4D);
    assert.strictEqual(buf.readUInt16LE(0), 0x5A4D);
    assert.strictEqual(cb.offset, 2);
  });

  it('should pad to alignment', () => {
    const buf = Buffer.alloc(64, 0);
    const cb = new CodeBuffer(buf, 0);
    cb.emit(0x90); // pos = 1
    cb.pad(4);     // pad to 4-byte boundary -> pos = 4
    assert.strictEqual(cb.offset, 4);
  });

  it('should report currentRVA for .text base', () => {
    const buf = Buffer.alloc(1024, 0);
    const cb = new CodeBuffer(buf, 0x200); // TEXT_RAW
    cb.emit(0x90, 0x90);
    assert.strictEqual(cb.currentRVA, TEXT_RVA + 2);
  });
});

// ─── X64Emitter ─────────────────────────────────────────────────────────────

describe('X64Emitter', () => {
  it('should emit standard prologue (sub rsp, 28h)', () => {
    const buf = Buffer.alloc(64, 0);
    const cb = new CodeBuffer(buf, 0);
    X64Emitter.emitPrologue(cb);
    assert.deepStrictEqual([...buf.subarray(0, 4)], [0x48, 0x83, 0xEC, 0x28]);
    assert.strictEqual(cb.offset, 4);
  });

  it('should emit standard epilogue (add rsp, 28h; ret)', () => {
    const buf = Buffer.alloc(64, 0);
    const cb = new CodeBuffer(buf, 0);
    X64Emitter.emitEpilogue(cb);
    assert.deepStrictEqual([...buf.subarray(0, 5)], [0x48, 0x83, 0xC4, 0x28, 0xC3]);
    assert.strictEqual(cb.offset, 5);
  });

  it('should emit frame-pointer prologue (push rbp; mov rbp,rsp; sub rsp,N)', () => {
    const buf = Buffer.alloc(64, 0);
    const cb = new CodeBuffer(buf, 0);
    X64Emitter.emitPrologueFP(cb, 0x40);
    assert.deepStrictEqual(
      [...buf.subarray(0, 8)],
      [0x55, 0x48, 0x89, 0xE5, 0x48, 0x83, 0xEC, 0x40]
    );
  });

  it('should emit frame-pointer epilogue (mov rsp,rbp; pop rbp; ret)', () => {
    const buf = Buffer.alloc(64, 0);
    const cb = new CodeBuffer(buf, 0);
    X64Emitter.emitEpilogueFP(cb);
    assert.deepStrictEqual(
      [...buf.subarray(0, 5)],
      [0x48, 0x89, 0xEC, 0x5D, 0xC3]
    );
  });

  it('should emit xor ecx, ecx (reg 1)', () => {
    const buf = Buffer.alloc(64, 0);
    const cb = new CodeBuffer(buf, 0);
    X64Emitter.emitXorReg32(cb, REG.RCX);
    assert.deepStrictEqual([...buf.subarray(0, 2)], [0x33, 0xC9]);
  });

  it('should emit xor eax, eax (reg 0)', () => {
    const buf = Buffer.alloc(64, 0);
    const cb = new CodeBuffer(buf, 0);
    X64Emitter.emitXorReg32(cb, REG.RAX);
    assert.deepStrictEqual([...buf.subarray(0, 2)], [0x33, 0xC0]);
  });

  it('should emit xor r9d, r9d (high reg)', () => {
    const buf = Buffer.alloc(64, 0);
    const cb = new CodeBuffer(buf, 0);
    X64Emitter.emitXorReg32High(cb, REG.R9);
    assert.deepStrictEqual([...buf.subarray(0, 3)], [0x45, 0x33, 0xC9]);
  });

  it('should emit call [rip+disp32]', () => {
    const buf = Buffer.alloc(64, 0);
    const cb = new CodeBuffer(buf, 0);
    X64Emitter.emitCallRipIndirect(cb, 0x00001050);
    assert.strictEqual(buf[0], 0xFF);
    assert.strictEqual(buf[1], 0x15);
    assert.strictEqual(buf.readUInt32LE(2), 0x1050);
    assert.strictEqual(cb.offset, 6);
  });

  it('should compute RIP displacement correctly', () => {
    // instrEnd at RVA 100Ch, target at RVA 205Ch => disp = 1050h
    assert.strictEqual(X64Emitter.ripDisp(0x100C, 0x205C), 0x1050);
    // Negative displacement
    assert.strictEqual(X64Emitter.ripDisp(0x2000, 0x1000), -0x1000);
  });

  it('should emit lea rdx, [rip+disp] (low reg)', () => {
    const buf = Buffer.alloc(64, 0);
    const cb = new CodeBuffer(buf, 0);
    X64Emitter.emitLeaRipReg(cb, REG.RDX, 0x10BB);
    assert.deepStrictEqual([...buf.subarray(0, 3)], [0x48, 0x8D, 0x15]);
    assert.strictEqual(buf.readUInt32LE(3), 0x10BB);
    assert.strictEqual(cb.offset, 7);
  });

  it('should emit lea r8, [rip+disp] (high reg)', () => {
    const buf = Buffer.alloc(64, 0);
    const cb = new CodeBuffer(buf, 0);
    X64Emitter.emitLeaRipReg(cb, REG.R8, 0x10A4);
    assert.deepStrictEqual([...buf.subarray(0, 3)], [0x4C, 0x8D, 0x05]);
    assert.strictEqual(buf.readUInt32LE(3), 0x10A4);
  });

  it('should emit mov ecx, imm32', () => {
    const buf = Buffer.alloc(64, 0);
    const cb = new CodeBuffer(buf, 0);
    X64Emitter.emitMovEcxImm32(cb, 42);
    assert.strictEqual(buf[0], 0xB9);
    assert.strictEqual(buf.readUInt32LE(1), 42);
    assert.strictEqual(cb.offset, 5);
  });

  it('should emit ret', () => {
    const buf = Buffer.alloc(64, 0);
    const cb = new CodeBuffer(buf, 0);
    X64Emitter.emitRet(cb);
    assert.strictEqual(buf[0], 0xC3);
  });

  it('should emit nop', () => {
    const buf = Buffer.alloc(64, 0);
    const cb = new CodeBuffer(buf, 0);
    X64Emitter.emitNop(cb);
    assert.strictEqual(buf[0], 0x90);
  });

  it('should emit int3', () => {
    const buf = Buffer.alloc(64, 0);
    const cb = new CodeBuffer(buf, 0);
    X64Emitter.emitInt3(cb);
    assert.strictEqual(buf[0], 0xCC);
  });

  it('should emit push/pop for low registers', () => {
    const buf = Buffer.alloc(64, 0);
    const cb = new CodeBuffer(buf, 0);
    X64Emitter.emitPushReg(cb, REG.RBX); // 53
    X64Emitter.emitPopReg(cb, REG.RBX);  // 5B
    assert.strictEqual(buf[0], 0x53);
    assert.strictEqual(buf[1], 0x5B);
  });

  it('should emit push/pop for high registers', () => {
    const buf = Buffer.alloc(64, 0);
    const cb = new CodeBuffer(buf, 0);
    X64Emitter.emitPushReg(cb, REG.R12); // 41 54
    X64Emitter.emitPopReg(cb, REG.R12);  // 41 5C
    assert.deepStrictEqual([...buf.subarray(0, 2)], [0x41, 0x54]);
    assert.deepStrictEqual([...buf.subarray(2, 4)], [0x41, 0x5C]);
  });

  it('should emit sub rsp with custom imm8', () => {
    const buf = Buffer.alloc(64, 0);
    const cb = new CodeBuffer(buf, 0);
    X64Emitter.emitSubRspImm8(cb, 0x38);
    assert.deepStrictEqual([...buf.subarray(0, 4)], [0x48, 0x83, 0xEC, 0x38]);
  });

  it('should emit add rsp with custom imm8', () => {
    const buf = Buffer.alloc(64, 0);
    const cb = new CodeBuffer(buf, 0);
    X64Emitter.emitAddRspImm8(cb, 0x38);
    assert.deepStrictEqual([...buf.subarray(0, 4)], [0x48, 0x83, 0xC4, 0x38]);
  });
});

// ─── PEBuilder Headers ──────────────────────────────────────────────────────

describe('PEBuilder — Headers', () => {
  it('should create a buffer of correct size for 3 sections', () => {
    const pe = new PEBuilder({ numSections: 3 });
    assert.strictEqual(pe.image.length, FILE_ALIGN * 4);
  });

  it('should create a buffer of correct size for 2 sections', () => {
    const pe = new PEBuilder({ numSections: 2 });
    assert.strictEqual(pe.image.length, FILE_ALIGN * 3);
  });

  it('should write valid DOS header (MZ + e_lfanew)', () => {
    const pe = new PEBuilder();
    pe.buildDOSHeader();
    assert.strictEqual(pe.image.readUInt16LE(0x00), 0x5A4D);
    assert.strictEqual(pe.image.readUInt32LE(0x3C), 0x80);
  });

  it('should write DOS stub bytes', () => {
    const pe = new PEBuilder();
    pe.buildDOSStub();
    assert.strictEqual(pe.image[0x40], 0xCD); // INT
    assert.strictEqual(pe.image[0x41], 0x21); // 21h
    assert.strictEqual(pe.image[0x42], 0xC3); // RET
  });

  it('should write PE signature at e_lfanew offset', () => {
    const pe = new PEBuilder();
    pe.buildDOSHeader();
    pe.buildNTHeaders();
    assert.strictEqual(pe.image.readUInt32LE(0x80), 0x00004550);
  });

  it('should write AMD64 machine type', () => {
    const pe = new PEBuilder();
    pe.buildNTHeaders();
    assert.strictEqual(pe.image.readUInt16LE(0x84), 0x8664);
  });

  it('should write PE32+ magic (020Bh)', () => {
    const pe = new PEBuilder();
    pe.buildNTHeaders();
    assert.strictEqual(pe.image.readUInt16LE(0x98), 0x020B);
  });

  it('should write correct ImageBase (140000000h)', () => {
    const pe = new PEBuilder();
    pe.buildNTHeaders();
    assert.strictEqual(pe.image.readUInt32LE(0xB0), 0x40000000);
    assert.strictEqual(pe.image.readUInt32LE(0xB4), 0x00000001);
  });

  it('should set CUI subsystem by default', () => {
    const pe = new PEBuilder();
    pe.buildNTHeaders();
    assert.strictEqual(pe.image.readUInt16LE(0xDC), 0x0003);
  });

  it('should set GUI subsystem when specified', () => {
    const pe = new PEBuilder({ subsystem: 0x0002 });
    pe.buildNTHeaders();
    assert.strictEqual(pe.image.readUInt16LE(0xDC), 0x0002);
  });

  it('should set correct entry point', () => {
    const pe = new PEBuilder({ entryRVA: 0x1000 });
    pe.buildNTHeaders();
    assert.strictEqual(pe.image.readUInt32LE(0xA8), 0x1000);
  });

  it('should set 16 data directory entries', () => {
    const pe = new PEBuilder();
    pe.buildNTHeaders();
    assert.strictEqual(pe.image.readUInt32LE(0x104), 16);
  });

  it('should write .text section header', () => {
    const pe = new PEBuilder();
    pe.buildSectionHeaders();
    // Name = ".text"
    const name = pe.image.subarray(0x188, 0x188 + 5).toString('ascii');
    assert.strictEqual(name, '.text');
    // VirtualAddress
    assert.strictEqual(pe.image.readUInt32LE(0x188 + 0x0C), TEXT_RVA);
    // Characteristics
    assert.strictEqual(pe.image.readUInt32LE(0x188 + 0x24), 0x60000020);
  });

  it('should write .rdata section header', () => {
    const pe = new PEBuilder();
    pe.buildSectionHeaders();
    const off = 0x188 + 40; // second section header
    const name = pe.image.subarray(off, off + 6).toString('ascii');
    assert.strictEqual(name, '.rdata');
    assert.strictEqual(pe.image.readUInt32LE(off + 0x0C), RDATA_RVA);
    assert.strictEqual(pe.image.readUInt32LE(off + 0x24), 0x40000040);
  });
});

// ─── PEBuilder Imports ──────────────────────────────────────────────────────

describe('PEBuilder — Imports', () => {
  function buildWithImports(imports) {
    const pe = new PEBuilder({ numSections: 3 });
    for (const [dll, fns] of imports) pe.addImport(dll, fns);
    pe.buildDOSHeader();
    pe.buildNTHeaders();
    pe.buildSectionHeaders();
    pe.buildImports();
    return pe;
  }

  it('should register single DLL import', () => {
    const pe = buildWithImports([['kernel32.dll', ['ExitProcess']]]);
    const iat = pe.getIATSlot('ExitProcess');
    assert.ok(iat >= RDATA_RVA, 'IAT slot should be in .rdata');
  });

  it('should register multi-DLL imports', () => {
    const pe = buildWithImports([
      ['kernel32.dll', ['ExitProcess', 'GetStdHandle']],
      ['user32.dll', ['MessageBoxA']]
    ]);
    const iatExit = pe.getIATSlot('ExitProcess');
    const iatGSH = pe.getIATSlot('GetStdHandle');
    const iatMB = pe.getIATSlot('MessageBoxA');

    assert.ok(iatExit >= RDATA_RVA);
    assert.ok(iatGSH >= RDATA_RVA);
    assert.ok(iatMB >= RDATA_RVA);

    // All slots must be distinct
    const slots = new Set([iatExit, iatGSH, iatMB]);
    assert.strictEqual(slots.size, 3, 'All IAT slots must be unique');
  });

  it('should throw for unknown import function', () => {
    const pe = new PEBuilder();
    assert.throws(() => pe.getIATSlot('FakeFunction'), /Import not found/);
  });

  it('should write import directory RVA in data directory', () => {
    const pe = buildWithImports([['kernel32.dll', ['ExitProcess']]]);
    assert.strictEqual(pe.image.readUInt32LE(0x110), RDATA_RVA);
    assert.ok(pe.image.readUInt32LE(0x114) > 0, 'Import dir size > 0');
  });

  it('should write IAT in data directory [12]', () => {
    const pe = buildWithImports([['kernel32.dll', ['ExitProcess']]]);
    const iatRVA = pe.image.readUInt32LE(0x168);
    assert.ok(iatRVA >= RDATA_RVA, 'IAT RVA should be in .rdata');
    assert.ok(pe.image.readUInt32LE(0x16C) > 0, 'IAT size > 0');
  });

  it('should add strings after imports', () => {
    const pe = buildWithImports([['kernel32.dll', ['ExitProcess']]]);
    const rva1 = pe.addString('Hello');
    const rva2 = pe.addString('World');
    assert.ok(rva1 >= RDATA_RVA);
    assert.ok(rva2 > rva1, 'Second string should be after first');
  });

  it('should deduplicate identical strings', () => {
    const pe = buildWithImports([['kernel32.dll', ['ExitProcess']]]);
    const rva1 = pe.addString('Test');
    const rva2 = pe.addString('Test');
    assert.strictEqual(rva1, rva2, 'Same string should return same RVA');
  });
});

// ─── End-to-End Compilation ─────────────────────────────────────────────────

describe('compileProgram — GUI target', () => {
  const outPath = path.join(__dirname, '_test_gui_output.exe');

  it('should produce a file with valid PE headers', () => {
    compileProgram({
      title: 'TestTitle',
      message: 'TestMessage',
      exitCode: 0,
      subsystem: 'gui'
    }, outPath);

    assert.ok(fs.existsSync(outPath), 'Output file should exist');

    const data = fs.readFileSync(outPath);
    // MZ signature
    assert.strictEqual(data.readUInt16LE(0), 0x5A4D);
    // PE signature
    const peOff = data.readUInt32LE(0x3C);
    assert.strictEqual(data.readUInt32LE(peOff), 0x00004550);
    // Machine = AMD64
    assert.strictEqual(data.readUInt16LE(peOff + 4), 0x8664);
    // PE32+ magic
    assert.strictEqual(data.readUInt16LE(peOff + 24), 0x020B);
    // Subsystem = GUI (0x0002)
    assert.strictEqual(data.readUInt16LE(peOff + 24 + 68), 0x0002);
    // Entry point = TEXT_RVA
    assert.strictEqual(data.readUInt32LE(peOff + 24 + 16), TEXT_RVA);

    fs.unlinkSync(outPath);
  });
});

describe('compileConsoleProgram — Console target', () => {
  const outPath = path.join(__dirname, '_test_console_output.exe');

  it('should produce a file with valid console PE headers', () => {
    compileConsoleProgram({
      message: 'Test console output\r\n'
    }, outPath);

    assert.ok(fs.existsSync(outPath), 'Output file should exist');

    const data = fs.readFileSync(outPath);
    // MZ
    assert.strictEqual(data.readUInt16LE(0), 0x5A4D);
    // PE
    const peOff = data.readUInt32LE(0x3C);
    assert.strictEqual(data.readUInt32LE(peOff), 0x00004550);
    // Subsystem = CUI (0x0003)
    assert.strictEqual(data.readUInt16LE(peOff + 24 + 68), 0x0003);
    // File must be larger than just headers
    assert.ok(data.length >= FILE_ALIGN * 3, 'File should have at least 3 sections');

    fs.unlinkSync(outPath);
  });

  it('should contain import table in .rdata', () => {
    compileConsoleProgram({
      message: 'Hello\r\n'
    }, outPath);

    const data = fs.readFileSync(outPath);
    const peOff = data.readUInt32LE(0x3C);
    // Data directory [1] = Import RVA
    const importRVA = data.readUInt32LE(peOff + 24 + 0x78);
    assert.strictEqual(importRVA, RDATA_RVA, 'Import dir should be at RDATA_RVA');
    const importSize = data.readUInt32LE(peOff + 24 + 0x7C);
    assert.ok(importSize > 0, 'Import dir size > 0');

    fs.unlinkSync(outPath);
  });
});

// ─── PE Image Structural Validation ─────────────────────────────────────────

describe('PE Image — Structural Integrity', () => {
  it('should have .text code starting at correct raw offset', () => {
    const outPath = path.join(__dirname, '_test_struct.exe');
    compileConsoleProgram({ message: 'X\r\n' }, outPath);
    const data = fs.readFileSync(outPath);

    // .text section header at offset 0x188
    const textRawOff = data.readUInt32LE(0x188 + 0x14);
    assert.strictEqual(textRawOff, 0x200, '.text raw offset should be 0x200');

    // First instruction should be sub rsp (48 83 EC ...)
    assert.strictEqual(data[textRawOff], 0x48, 'First byte of .text should be REX.W');
    assert.strictEqual(data[textRawOff + 1], 0x83, 'Second byte should be 83h (sub/add)');

    fs.unlinkSync(outPath);
  });

  it('should have section alignment = 1000h', () => {
    const pe = new PEBuilder();
    pe.buildNTHeaders();
    assert.strictEqual(pe.image.readUInt32LE(0xB8), SECT_ALIGN);
  });

  it('should have file alignment = 200h', () => {
    const pe = new PEBuilder();
    pe.buildNTHeaders();
    assert.strictEqual(pe.image.readUInt32LE(0xBC), FILE_ALIGN);
  });

  it('should have EXECUTABLE_IMAGE | LARGE_ADDRESS_AWARE characteristics', () => {
    const pe = new PEBuilder();
    pe.buildNTHeaders();
    const chars = pe.image.readUInt16LE(0x96);
    assert.ok(chars & 0x0002, 'EXECUTABLE_IMAGE should be set');
    assert.ok(chars & 0x0020, 'LARGE_ADDRESS_AWARE should be set');
  });

  it('should have NX_COMPAT and DYNAMIC_BASE in DllCharacteristics', () => {
    const pe = new PEBuilder();
    pe.buildNTHeaders();
    const dllChars = pe.image.readUInt16LE(0xDE);
    assert.ok(dllChars & 0x0040, 'DYNAMIC_BASE should be set');
    assert.ok(dllChars & 0x0100, 'NX_COMPAT should be set');
  });
});

// ─── CompileService Integration ─────────────────────────────────────────────

describe('CompileService', () => {
  it('should be importable', () => {
    const { CompileService } = require('../src/services/compile-service');
    assert.ok(CompileService);
  });

  it('should compile with default console target', () => {
    const { CompileService } = require('../src/services/compile-service');
    const outDir = __dirname;
    const svc = new CompileService({});
    const result = svc.compile({ target: 'console', outputDir: outDir });
    assert.strictEqual(result.success, true);
    assert.ok(fs.existsSync(result.outputPath));
    fs.unlinkSync(result.outputPath);
  });

  it('should compile with GUI target', () => {
    const { CompileService } = require('../src/services/compile-service');
    const outDir = __dirname;
    const svc = new CompileService({});
    const result = svc.compile({ target: 'gui', outputDir: outDir });
    assert.strictEqual(result.success, true);
    assert.ok(fs.existsSync(result.outputPath));
    fs.unlinkSync(result.outputPath);
  });

  it('should return compileToBuffer result', () => {
    const { CompileService } = require('../src/services/compile-service');
    const svc = new CompileService({});
    const result = svc.compileToBuffer({ target: 'console' });
    assert.strictEqual(result.success, true);
    assert.ok(Buffer.isBuffer(result.buffer));
    assert.strictEqual(result.buffer.readUInt16LE(0), 0x5A4D);
  });
});

// ─── Extension Registration ─────────────────────────────────────────────────

describe('PE Backend Extension', () => {
  it('should export activate and deactivate', () => {
    const ext = require('../src/extensions/pe-backend-extension');
    assert.strictEqual(typeof ext.activate, 'function');
    assert.strictEqual(typeof ext.deactivate, 'function');
  });

  it('should register commands on activation', () => {
    const ext = require('../src/extensions/pe-backend-extension');
    const commands = {};
    const keybindings = {};
    const menus = {};
    const ide = {
      registerCommand: (name, fn) => { commands[name] = fn; },
      registerKeybinding: (key, cmd) => { keybindings[key] = cmd; },
      registerMenu: (label, items) => { menus[label] = items; },
      log: () => { }
    };
    ext.activate(ide);

    assert.ok(commands['rawrxd.compileConsole'], 'compileConsole should be registered');
    assert.ok(commands['rawrxd.compileGUI'], 'compileGUI should be registered');
    assert.ok(commands['rawrxd.compileAndRun'], 'compileAndRun should be registered');
    assert.strictEqual(keybindings['F5'], 'rawrxd.compileAndRun');
    assert.strictEqual(keybindings['Ctrl+Shift+B'], 'rawrxd.compileConsole');
    assert.ok(menus['Build'], 'Build menu should be registered');
    assert.ok(menus['Build'].length >= 3, 'Build menu should have entries');
  });
});
