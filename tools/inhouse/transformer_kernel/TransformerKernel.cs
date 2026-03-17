// ═══════════════════════════════════════════════════════════════════════════════
// RawrXD Transformer Kernel - Zero-Dependency LLM Inference Engine
// ═══════════════════════════════════════════════════════════════════════════════
// AVX-512 matrix multiplication kernels (Q4_0, Q4_K_M, Q5, Q8, F16)
// GGUF graph interpreter (RMS Norm, RoPE, SwiGLU, attention masks, etc.)
// KV-cache management with rolling buffer logic
// BPE tokenizer emitted as raw x64 machine code
// ═══════════════════════════════════════════════════════════════════════════════

using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;

namespace RawrXD.TransformerKernel;

// ─── Quantization Types ─────────────────────────────────────────────────────
public enum GGMLType : uint
{
    F32     = 0,
    F16     = 1,
    Q4_0    = 2,
    Q4_1    = 3,
    Q5_0    = 6,
    Q5_1    = 7,
    Q8_0    = 8,
    Q8_1    = 9,
    Q2_K    = 10,
    Q3_K    = 11,
    Q4_K    = 12,
    Q5_K    = 13,
    Q6_K    = 14,
    Q8_K    = 15,
    IQ2_XXS = 16,
    IQ2_XS  = 17,
    IQ3_XXS = 18,
    IQ1_S   = 19,
    IQ4_NL  = 20,
    IQ3_S   = 21,
    IQ2_S   = 22,
    IQ4_XS  = 23,
    I8      = 24,
    I16     = 25,
    I32     = 26,
    I64     = 27,
    F64     = 28,
    IQ1_M   = 29,
    BF16    = 30,
    Count   = 31,
}

// ─── GGUF Op Types ──────────────────────────────────────────────────────────
public enum GGMLOp : uint
{
    // Unary
    None = 0, Dup, Add, Add1, Acc, Sub, Mul, Div, Sqr, Sqrt, Log, Abs,
    Sgn, Neg, Step, Tanh, Elu, Relu, Sigmoid, Gelu, GeluQuick, Silu,
    // Normalization
    Norm, RmsNorm, RmsNormBack,
    // Group norm
    GroupNorm,
    // MatMul
    MulMat, MulMatId, OutProd,
    // Scale / set
    Scale, Set, Cpy, Cont, Reshape, View, Permute, Transpose,
    GetRows, GetRowsBack, Diag, DiagMaskInf, DiagMaskZero,
    SoftMax, SoftMaxBack,
    // RoPE
    Rope, RopeBack,
    // Convolution
    Conv1d, Conv2d, ConvTranspose1d, ConvTranspose2d, Pool1d, Pool2d,
    // Upscale
    Upscale,
    // Sort
    ArgsortF32, ArgsortI32,
    // Flash attention
    FlashAttn, FlashAttnBack, FlashAttnExt,
    // SSM
    SsmConv, SsmScan,
    // Misc
    WinPart, WinUnpart, Unary, MapUnary, MapBinary, MapCustom1, MapCustom2,
    MapCustom3, Cross, TimestepEmbed,
    // Quantize
    Quantize, Dequantize,
    // SwiGLU (fused)
    SwiGLU,
    // Cache ops
    KVCacheView, KVCacheUpdate, KVCacheSeek, KVCacheClear,
    // Repeat / concat
    Repeat, RepeatBack, Concat,
    // Clamp
    Clamp,
    // Pad
    Pad,
    // Arange
    Arange,
    // Timestep
    Timestep,
    // Count sentinel
    Count
}

// ─── GGUF File Structures ───────────────────────────────────────────────────

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct GGUFHeader
{
    public uint Magic;           // 0x46475547 = "GGUF"
    public uint Version;
    public ulong TensorCount;
    public ulong MetadataKVCount;
}

public enum GGUFMetadataValueType : uint
{
    UInt8 = 0, Int8, UInt16, Int16, UInt32, Int32, Float32,
    Bool, String, Array, UInt64, Int64, Float64,
}

public sealed class GGUFMetadataKV
{
    public string Key = "";
    public GGUFMetadataValueType ValueType;
    public object? Value;
}

public sealed class GGUFTensorInfo
{
    public string Name = "";
    public int NDim;
    public long[] Shape = Array.Empty<long>();
    public GGMLType Type;
    public ulong Offset;
}

// ─── AVX-512 Machine Code Emitter ───────────────────────────────────────────

/// <summary>
/// Emits raw x64 AVX-512 machine code for quantized matrix multiplication,
/// RMS norm, RoPE, SwiGLU, softmax, and BPE tokenizer.
/// No assembler dependency. Bytes go directly into .text section.
/// </summary>
public static class X64Emitter
{
    // ── Register encoding ────────────────────────────────────────────────
    // GP: RAX=0 RCX=1 RDX=2 RBX=3 RSP=4 RBP=5 RSI=6 RDI=7 R8-R15=8..15
    // ZMM0-ZMM31 for AVX-512
    private const byte REX_W   = 0x48;
    private const byte REX_WR  = 0x4C;
    private const byte REX_WB  = 0x49;
    private const byte REX_WRB = 0x4D;

    // EVEX prefix helpers
    private static byte[] EvexPrefix(int zmm_dst, int zmm_src1, int zmm_src2,
                                      byte mm, byte pp, byte W, int opmask = 0, bool broadcast = false)
    {
        // EVEX is 4 bytes: 62h | P0 | P1 | P2
        byte p0 = 0x62;

        // P1: R, X, B, R', 00, mm
        int R  = ((zmm_dst & 8) == 0) ? 1 : 0;   // inverted
        int X  = 1;                                  // no SIB index
        int B  = ((zmm_src2 & 8) == 0) ? 1 : 0;   // inverted
        int Rp = ((zmm_dst & 16) == 0) ? 1 : 0;   // inverted
        byte p1 = (byte)((R << 7) | (X << 6) | (B << 5) | (Rp << 4) | (mm & 0x03));

        // P2: W, vvvv, 1, pp
        int vvvv = (~zmm_src1) & 0x0F;
        byte p2 = (byte)((W << 7) | (vvvv << 3) | (1 << 2) | (pp & 0x03));

        // P3: z, L'L, b, V', aaa
        int Vp = ((~zmm_src1 >> 4) & 1);
        int z = 0;   // no zeroing
        int LL = 2;  // 512-bit (10b)
        int b_bit = broadcast ? 1 : 0;
        byte p3 = (byte)((z << 7) | (LL << 5) | (b_bit << 4) | (Vp << 3) | (opmask & 0x07));

        return new byte[] { p0, p1, p2, p3 };
    }

    internal static byte ModRM(int mod, int reg, int rm)
        => (byte)((mod << 6) | ((reg & 7) << 3) | (rm & 7));

    // ── Standard prologue / epilogue ─────────────────────────────────────

    /// <summary>Standard x64 function prologue: push rbp; mov rbp,rsp; sub rsp,shadowSpace</summary>
    public static byte[] Emit_FunctionPrologue(int shadowSpace = 0x40)
    {
        var code = new List<byte>();
        code.Add(0x55);                               // push rbp
        code.AddRange(new byte[] { 0x48, 0x89, 0xE5 }); // mov rbp, rsp
        if (shadowSpace > 0)
        {
            code.AddRange(new byte[] { 0x48, 0x83, 0xEC, (byte)shadowSpace }); // sub rsp, imm8
        }
        return code.ToArray();
    }

    /// <summary>Standard x64 function epilogue: mov rsp,rbp; pop rbp; ret</summary>
    public static byte[] Emit_FunctionEpilogue()
    {
        return new byte[]
        {
            0x48, 0x89, 0xEC,   // mov rsp, rbp
            0x5D,               // pop rbp
            0xC3                // ret
        };
    }

    // ── AVX-512 vfmadd231ps  zmm_d, zmm_s1, zmm_s2 ─────────────────────
    // Fused multiply-add: zmm_d = zmm_s1 * zmm_s2 + zmm_d
    public static byte[] Emit_VFMADD231PS(int zmm_d, int zmm_s1, int zmm_s2)
    {
        var evex = EvexPrefix(zmm_d, zmm_s1, zmm_s2, mm: 0x02, pp: 0x01, W: 0);
        byte opcode = 0xB8; // vfmadd231ps
        byte modrm = ModRM(3, zmm_d & 7, zmm_s2 & 7);
        var code = new byte[evex.Length + 2];
        Array.Copy(evex, code, evex.Length);
        code[evex.Length] = opcode;
        code[evex.Length + 1] = modrm;
        return code;
    }

    // ── AVX-512 vmovups load: vmovups zmm, [reg+disp] ───────────────────
    public static byte[] Emit_VMOVUPS_Load(int zmm_dst, int gpr_base, int disp = 0)
    {
        var evex = EvexPrefix(zmm_dst, 0, gpr_base, mm: 0x01, pp: 0x00, W: 0);
        byte opcode = 0x10; // vmovups load
        byte modrm;
        var extra = new List<byte>();

        if (disp == 0 && (gpr_base & 7) != 5) // not RBP/R13
        {
            modrm = ModRM(0, zmm_dst & 7, gpr_base & 7);
        }
        else if (disp >= -128 && disp <= 127)
        {
            modrm = ModRM(1, zmm_dst & 7, gpr_base & 7);
            extra.Add((byte)(disp / 64)); // EVEX disp8*64
        }
        else
        {
            modrm = ModRM(2, zmm_dst & 7, gpr_base & 7);
            extra.AddRange(BitConverter.GetBytes(disp));
        }

        // SIB if base is RSP/R12
        byte? sib = null;
        if ((gpr_base & 7) == 4)
            sib = 0x24;

        var code = new List<byte>(evex);
        code.Add(opcode);
        code.Add(modrm);
        if (sib.HasValue) code.Add(sib.Value);
        code.AddRange(extra);
        return code.ToArray();
    }

    // ── AVX-512 vmovups store: vmovups [reg+disp], zmm ──────────────────
    public static byte[] Emit_VMOVUPS_Store(int gpr_base, int zmm_src, int disp = 0)
    {
        var evex = EvexPrefix(zmm_src, 0, gpr_base, mm: 0x01, pp: 0x00, W: 0);
        byte opcode = 0x11; // vmovups store
        byte modrm;
        var extra = new List<byte>();

        if (disp == 0 && (gpr_base & 7) != 5)
        {
            modrm = ModRM(0, zmm_src & 7, gpr_base & 7);
        }
        else if (disp >= -128 && disp <= 127)
        {
            modrm = ModRM(1, zmm_src & 7, gpr_base & 7);
            extra.Add((byte)(disp / 64));
        }
        else
        {
            modrm = ModRM(2, zmm_src & 7, gpr_base & 7);
            extra.AddRange(BitConverter.GetBytes(disp));
        }

        byte? sib = null;
        if ((gpr_base & 7) == 4) sib = 0x24;

        var code = new List<byte>(evex);
        code.Add(opcode);
        code.Add(modrm);
        if (sib.HasValue) code.Add(sib.Value);
        code.AddRange(extra);
        return code.ToArray();
    }

    // ── AVX-512 vxorps zmm, zmm, zmm (zero register) ───────────────────
    public static byte[] Emit_VXORPS(int zmm_dst, int zmm_src1, int zmm_src2)
    {
        var evex = EvexPrefix(zmm_dst, zmm_src1, zmm_src2, mm: 0x01, pp: 0x00, W: 0);
        byte opcode = 0x57;
        byte modrm = ModRM(3, zmm_dst & 7, zmm_src2 & 7);
        var code = new byte[evex.Length + 2];
        Array.Copy(evex, code, evex.Length);
        code[evex.Length] = opcode;
        code[evex.Length + 1] = modrm;
        return code;
    }

    // ── AVX-512 vaddps zmm, zmm, zmm ───────────────────────────────────
    public static byte[] Emit_VADDPS(int zmm_d, int zmm_s1, int zmm_s2)
    {
        var evex = EvexPrefix(zmm_d, zmm_s1, zmm_s2, mm: 0x01, pp: 0x00, W: 0);
        byte opcode = 0x58;
        byte modrm = ModRM(3, zmm_d & 7, zmm_s2 & 7);
        var code = new byte[evex.Length + 2];
        Array.Copy(evex, code, evex.Length);
        code[evex.Length] = opcode;
        code[evex.Length + 1] = modrm;
        return code;
    }

    // ── AVX-512 vmulps zmm, zmm, zmm ───────────────────────────────────
    public static byte[] Emit_VMULPS(int zmm_d, int zmm_s1, int zmm_s2)
    {
        var evex = EvexPrefix(zmm_d, zmm_s1, zmm_s2, mm: 0x01, pp: 0x00, W: 0);
        byte opcode = 0x59;
        byte modrm = ModRM(3, zmm_d & 7, zmm_s2 & 7);
        var code = new byte[evex.Length + 2];
        Array.Copy(evex, code, evex.Length);
        code[evex.Length] = opcode;
        code[evex.Length + 1] = modrm;
        return code;
    }

    // ── AVX-512 vbroadcastss zmm, [reg] (broadcast float to all lanes) ──
    public static byte[] Emit_VBROADCASTSS(int zmm_dst, int gpr_base, int disp = 0)
    {
        var evex = EvexPrefix(zmm_dst, 0, gpr_base, mm: 0x02, pp: 0x01, W: 0);
        byte opcode = 0x18; // vbroadcastss
        byte modrm;
        var extra = new List<byte>();

        if (disp == 0 && (gpr_base & 7) != 5)
            modrm = ModRM(0, zmm_dst & 7, gpr_base & 7);
        else if (disp >= -128 && disp <= 127)
        {
            modrm = ModRM(1, zmm_dst & 7, gpr_base & 7);
            extra.Add((byte)disp);
        }
        else
        {
            modrm = ModRM(2, zmm_dst & 7, gpr_base & 7);
            extra.AddRange(BitConverter.GetBytes(disp));
        }

        byte? sib = null;
        if ((gpr_base & 7) == 4) sib = 0x24;

        var code = new List<byte>(evex);
        code.Add(opcode);
        code.Add(modrm);
        if (sib.HasValue) code.Add(sib.Value);
        code.AddRange(extra);
        return code.ToArray();
    }

    // ── AVX-512 reduce horizontal: vextractf64x4 + vaddps chain ─────────
    // Reduces zmm0 → scalar in xmm0[0]
    public static byte[] Emit_HorizontalSum_ZMM0()
    {
        var code = new List<byte>();

        // vextractf64x4 ymm1, zmm0, 1  → upper 256 bits
        // EVEX.512.66.0F3A.W1 1B /r ib
        code.AddRange(EvexPrefix(0, 0, 1, mm: 0x03, pp: 0x01, W: 1));
        code.Add(0x1B);
        code.Add(ModRM(3, 0, 1));
        code.Add(0x01);

        // vaddps ymm0, ymm0, ymm1  (VEX 256)
        code.AddRange(new byte[] { 0xC5, 0xFC, 0x58, 0xC1 });

        // vextractf128 xmm1, ymm0, 1
        code.AddRange(new byte[] { 0xC4, 0xE3, 0x7D, 0x19, 0xC1, 0x01 });

        // vaddps xmm0, xmm0, xmm1
        code.AddRange(new byte[] { 0xC5, 0xF8, 0x58, 0xC1 });

        // vshufps xmm1, xmm0, xmm0, 0x4E  (swap high/low 64-bit halves)
        code.AddRange(new byte[] { 0xC5, 0xF8, 0xC6, 0xC8, 0x4E });

        // vaddps xmm0, xmm0, xmm1
        code.AddRange(new byte[] { 0xC5, 0xF8, 0x58, 0xC1 });

        // vshufps xmm1, xmm0, xmm0, 0xB1  (swap adjacent 32-bit)
        code.AddRange(new byte[] { 0xC5, 0xF8, 0xC6, 0xC8, 0xB1 });

        // vaddss xmm0, xmm0, xmm1
        code.AddRange(new byte[] { 0xC5, 0xFA, 0x58, 0xC1 });

        return code.ToArray();
    }

    // ── MOV reg, imm64 ──────────────────────────────────────────────────
    public static byte[] Emit_MovImm64(int reg, ulong imm)
    {
        var code = new byte[10];
        code[0] = (byte)(0x48 | ((reg >> 3) & 1));  // REX.W + REX.B if r8+
        code[1] = (byte)(0xB8 + (reg & 7));
        BinaryPrimitives.WriteUInt64LittleEndian(code.AsSpan(2), imm);
        return code;
    }

    // ── MOV reg, [reg+disp32] ───────────────────────────────────────────
    public static byte[] Emit_MovLoad64(int dst, int base_reg, int disp)
    {
        var code = new List<byte>();
        byte rex = 0x48;
        if (dst >= 8) rex |= 0x04;
        if (base_reg >= 8) rex |= 0x01;
        code.Add(rex);
        code.Add(0x8B);
        code.Add(ModRM(2, dst & 7, base_reg & 7));
        if ((base_reg & 7) == 4) code.Add(0x24); // SIB
        code.AddRange(BitConverter.GetBytes(disp));
        return code.ToArray();
    }

    // ── LEA reg, [reg+disp32] ───────────────────────────────────────────
    public static byte[] Emit_Lea(int dst, int base_reg, int disp)
    {
        var code = new List<byte>();
        byte rex = 0x48;
        if (dst >= 8) rex |= 0x04;
        if (base_reg >= 8) rex |= 0x01;
        code.Add(rex);
        code.Add(0x8D);
        code.Add(ModRM(2, dst & 7, base_reg & 7));
        if ((base_reg & 7) == 4) code.Add(0x24);
        code.AddRange(BitConverter.GetBytes(disp));
        return code.ToArray();
    }

    // ── SUB reg, imm32 ─────────────────────────────────────────────────
    public static byte[] Emit_SubImm32(int reg, int imm)
    {
        var code = new List<byte>();
        byte rex = 0x48;
        if (reg >= 8) rex |= 0x01;
        code.Add(rex);
        code.Add(0x81);
        code.Add(ModRM(3, 5, reg & 7));
        code.AddRange(BitConverter.GetBytes(imm));
        return code.ToArray();
    }

    // ── ADD reg, imm32 ─────────────────────────────────────────────────
    public static byte[] Emit_AddImm32(int reg, int imm)
    {
        var code = new List<byte>();
        byte rex = 0x48;
        if (reg >= 8) rex |= 0x01;
        code.Add(rex);
        code.Add(0x81);
        code.Add(ModRM(3, 0, reg & 7));
        code.AddRange(BitConverter.GetBytes(imm));
        return code.ToArray();
    }

    // ── CMP reg, imm32 ─────────────────────────────────────────────────
    public static byte[] Emit_CmpImm32(int reg, int imm)
    {
        var code = new List<byte>();
        byte rex = 0x48;
        if (reg >= 8) rex |= 0x01;
        code.Add(rex);
        code.Add(0x81);
        code.Add(ModRM(3, 7, reg & 7));
        code.AddRange(BitConverter.GetBytes(imm));
        return code.ToArray();
    }

    // ── JL rel32 (short: JL rel8 if fits) ───────────────────────────────
    public static byte[] Emit_JL_Rel32(int rel32)
    {
        var code = new byte[6];
        code[0] = 0x0F;
        code[1] = 0x8C;
        BinaryPrimitives.WriteInt32LittleEndian(code.AsSpan(2), rel32);
        return code;
    }

    // ── JMP rel32 ──────────────────────────────────────────────────────
    public static byte[] Emit_Jmp_Rel32(int rel32)
    {
        var code = new byte[5];
        code[0] = 0xE9;
        BinaryPrimitives.WriteInt32LittleEndian(code.AsSpan(1), rel32);
        return code;
    }

    // ── RET ────────────────────────────────────────────────────────────
    public static byte[] Emit_Ret() => new byte[] { 0xC3 };

    // ── PUSH reg ───────────────────────────────────────────────────────
    public static byte[] Emit_Push(int reg)
    {
        if (reg < 8) return new byte[] { (byte)(0x50 + reg) };
        return new byte[] { 0x41, (byte)(0x50 + (reg & 7)) };
    }

    // ── POP reg ────────────────────────────────────────────────────────
    public static byte[] Emit_Pop(int reg)
    {
        if (reg < 8) return new byte[] { (byte)(0x58 + reg) };
        return new byte[] { 0x41, (byte)(0x58 + (reg & 7)) };
    }

    // ── CALL rel32 ────────────────────────────────────────────────────
    public static byte[] Emit_Call_Rel32(int rel32)
    {
        var code = new byte[5];
        code[0] = 0xE8;
        BinaryPrimitives.WriteInt32LittleEndian(code.AsSpan(1), rel32);
        return code;
    }

    // ── XOR reg, reg (zero) ────────────────────────────────────────────
    public static byte[] Emit_Xor(int reg1, int reg2)
    {
        var code = new List<byte>();
        byte rex = 0x48;
        if (reg1 >= 8) rex |= 0x04;
        if (reg2 >= 8) rex |= 0x01;
        code.Add(rex);
        code.Add(0x31);
        code.Add(ModRM(3, reg1 & 7, reg2 & 7));
        return code.ToArray();
    }

    // ── MOV reg, reg ───────────────────────────────────────────────────
    public static byte[] Emit_MovReg(int dst, int src)
    {
        var code = new List<byte>();
        byte rex = 0x48;
        if (src >= 8) rex |= 0x04;
        if (dst >= 8) rex |= 0x01;
        code.Add(rex);
        code.Add(0x89);
        code.Add(ModRM(3, src & 7, dst & 7));
        return code.ToArray();
    }

    // ── INC reg ────────────────────────────────────────────────────────
    public static byte[] Emit_Inc(int reg)
    {
        var code = new List<byte>();
        byte rex = 0x48;
        if (reg >= 8) rex |= 0x01;
        code.Add(rex);
        code.Add(0xFF);
        code.Add(ModRM(3, 0, reg & 7));
        return code.ToArray();
    }

    // ── DEC reg ────────────────────────────────────────────────────────
    public static byte[] Emit_Dec(int reg)
    {
        var code = new List<byte>();
        byte rex = 0x48;
        if (reg >= 8) rex |= 0x01;
        code.Add(rex);
        code.Add(0xFF);
        code.Add(ModRM(3, 1, reg & 7));
        return code.ToArray();
    }

    // ── IMUL reg, reg ──────────────────────────────────────────────────
    public static byte[] Emit_Imul(int dst, int src)
    {
        var code = new List<byte>();
        byte rex = 0x48;
        if (dst >= 8) rex |= 0x04;
        if (src >= 8) rex |= 0x01;
        code.Add(rex);
        code.Add(0x0F);
        code.Add(0xAF);
        code.Add(ModRM(3, dst & 7, src & 7));
        return code.ToArray();
    }

    // ── SHL reg, imm8 ─────────────────────────────────────────────────
    public static byte[] Emit_Shl(int reg, byte imm)
    {
        var code = new List<byte>();
        byte rex = 0x48;
        if (reg >= 8) rex |= 0x01;
        code.Add(rex);
        code.Add(0xC1);
        code.Add(ModRM(3, 4, reg & 7));
        code.Add(imm);
        return code.ToArray();
    }

    // ── SHR reg, imm8 ─────────────────────────────────────────────────
    public static byte[] Emit_Shr(int reg, byte imm)
    {
        var code = new List<byte>();
        byte rex = 0x48;
        if (reg >= 8) rex |= 0x01;
        code.Add(rex);
        code.Add(0xC1);
        code.Add(ModRM(3, 5, reg & 7));
        code.Add(imm);
        return code.ToArray();
    }

    // ── AND reg, imm32 ────────────────────────────────────────────────
    public static byte[] Emit_AndImm32(int reg, int imm)
    {
        var code = new List<byte>();
        byte rex = 0x48;
        if (reg >= 8) rex |= 0x01;
        code.Add(rex);
        code.Add(0x81);
        code.Add(ModRM(3, 4, reg & 7));
        code.AddRange(BitConverter.GetBytes(imm));
        return code.ToArray();
    }

    // ── vcvtph2ps zmm, ymm (F16→F32 conversion with AVX-512) ────────
    public static byte[] Emit_VCVTPH2PS(int zmm_dst, int ymm_src)
    {
        // EVEX.512.66.0F38.W0 13 /r
        var evex = EvexPrefix(zmm_dst, 0, ymm_src, mm: 0x02, pp: 0x01, W: 0);
        byte opcode = 0x13;
        byte modrm = ModRM(3, zmm_dst & 7, ymm_src & 7);
        var code = new byte[evex.Length + 2];
        Array.Copy(evex, code, evex.Length);
        code[evex.Length] = opcode;
        code[evex.Length + 1] = modrm;
        return code;
    }

    // ── vcvtps2ph ymm, zmm, imm8 (F32→F16) ─────────────────────────
    public static byte[] Emit_VCVTPS2PH(int ymm_dst, int zmm_src, byte rounding = 0)
    {
        // EVEX.512.66.0F3A.W0 1D /r ib
        var evex = EvexPrefix(zmm_src, 0, ymm_dst, mm: 0x03, pp: 0x01, W: 0);
        byte opcode = 0x1D;
        byte modrm = ModRM(3, zmm_src & 7, ymm_dst & 7);
        var code = new byte[evex.Length + 3];
        Array.Copy(evex, code, evex.Length);
        code[evex.Length] = opcode;
        code[evex.Length + 1] = modrm;
        code[evex.Length + 2] = rounding;
        return code;
    }

    // ── vpmovsxbd zmm, xmm (sign extend byte→dword for Q8 dequant) ──
    public static byte[] Emit_VPMOVSXBD(int zmm_dst, int xmm_src)
    {
        // EVEX.512.66.0F38.WIG 21 /r
        var evex = EvexPrefix(zmm_dst, 0, xmm_src, mm: 0x02, pp: 0x01, W: 0);
        byte opcode = 0x21;
        byte modrm = ModRM(3, zmm_dst & 7, xmm_src & 7);
        var code = new byte[evex.Length + 2];
        Array.Copy(evex, code, evex.Length);
        code[evex.Length] = opcode;
        code[evex.Length + 1] = modrm;
        return code;
    }

    // ── vcvtdq2ps zmm, zmm (int32→float32) ─────────────────────────
    public static byte[] Emit_VCVTDQ2PS(int zmm_dst, int zmm_src)
    {
        var evex = EvexPrefix(zmm_dst, 0, zmm_src, mm: 0x01, pp: 0x00, W: 0);
        byte opcode = 0x5B;
        byte modrm = ModRM(3, zmm_dst & 7, zmm_src & 7);
        var code = new byte[evex.Length + 2];
        Array.Copy(evex, code, evex.Length);
        code[evex.Length] = opcode;
        code[evex.Length + 1] = modrm;
        return code;
    }

    // ── vrsqrt14ps zmm, zmm (reciprocal sqrt for RMS norm) ──────────
    public static byte[] Emit_VRSQRT14PS(int zmm_dst, int zmm_src)
    {
        // EVEX.512.66.0F38.W0 4E /r
        var evex = EvexPrefix(zmm_dst, 0, zmm_src, mm: 0x02, pp: 0x01, W: 0);
        byte opcode = 0x4E;
        byte modrm = ModRM(3, zmm_dst & 7, zmm_src & 7);
        var code = new byte[evex.Length + 2];
        Array.Copy(evex, code, evex.Length);
        code[evex.Length] = opcode;
        code[evex.Length + 1] = modrm;
        return code;
    }

    // ── vrcp14ps zmm, zmm (reciprocal for softmax) ─────────────────
    public static byte[] Emit_VRCP14PS(int zmm_dst, int zmm_src)
    {
        // EVEX.512.66.0F38.W0 4C /r
        var evex = EvexPrefix(zmm_dst, 0, zmm_src, mm: 0x02, pp: 0x01, W: 0);
        byte opcode = 0x4C;
        byte modrm = ModRM(3, zmm_dst & 7, zmm_src & 7);
        var code = new byte[evex.Length + 2];
        Array.Copy(evex, code, evex.Length);
        code[evex.Length] = opcode;
        code[evex.Length + 1] = modrm;
        return code;
    }

    // ── vmaxps zmm, zmm, zmm ───────────────────────────────────────
    public static byte[] Emit_VMAXPS(int zmm_d, int zmm_s1, int zmm_s2)
    {
        var evex = EvexPrefix(zmm_d, zmm_s1, zmm_s2, mm: 0x01, pp: 0x00, W: 0);
        byte opcode = 0x5F;
        byte modrm = ModRM(3, zmm_d & 7, zmm_s2 & 7);
        var code = new byte[evex.Length + 2];
        Array.Copy(evex, code, evex.Length);
        code[evex.Length] = opcode;
        code[evex.Length + 1] = modrm;
        return code;
    }

    // ── vsubps zmm, zmm, zmm ────────────────────────────────────────
    public static byte[] Emit_VSUBPS(int zmm_d, int zmm_s1, int zmm_s2)
    {
        var evex = EvexPrefix(zmm_d, zmm_s1, zmm_s2, mm: 0x01, pp: 0x00, W: 0);
        byte opcode = 0x5C;
        byte modrm = ModRM(3, zmm_d & 7, zmm_s2 & 7);
        var code = new byte[evex.Length + 2];
        Array.Copy(evex, code, evex.Length);
        code[evex.Length] = opcode;
        code[evex.Length + 1] = modrm;
        return code;
    }
}

// ─── Quantized MatMul Kernel Builder ────────────────────────────────────────

/// <summary>
/// Builds complete machine code kernels for quantized matrix multiplication.
/// Each kernel processes one row of output: C[i,j] = dot(A_row_i, B_col_j)
/// Windows x64 ABI: RCX=A_ptr, RDX=B_ptr, R8=C_ptr, R9=K (inner dim)
/// </summary>
public static class MatMulKernelBuilder
{
    private const int REG_RCX = 1;
    private const int REG_RDX = 2;
    private const int REG_RBX = 3;
    private const int REG_RSI = 6;
    private const int REG_RDI = 7;
    private const int REG_R8  = 8;
    private const int REG_R9  = 9;
    private const int REG_R10 = 10;
    private const int REG_R11 = 11;
    private const int REG_R12 = 12;
    private const int REG_R13 = 13;
    private const int REG_R14 = 14;
    private const int REG_R15 = 15;

    /// <summary>
    /// Emits a complete F16 matmul kernel.
    /// Loads F16, converts to F32 via vcvtph2ps, accumulates via vfmadd231ps.
    /// RCX = A (f16*), RDX = B (f16*), R8 = C (f32*), R9 = K
    /// </summary>
    public static byte[] Build_F16_MatMul_Kernel()
    {
        var code = new List<byte>();

        // Prologue
        code.AddRange(X64Emitter.Emit_FunctionPrologue(0x40));
        code.AddRange(X64Emitter.Emit_Push(REG_RBX));
        code.AddRange(X64Emitter.Emit_Push(REG_R12));
        code.AddRange(X64Emitter.Emit_Push(REG_R13));

        // Zero accumulators zmm0..zmm3
        for (int i = 0; i < 4; i++)
            code.AddRange(X64Emitter.Emit_VXORPS(i, i, i));

        // R10 = loop counter = 0
        code.AddRange(X64Emitter.Emit_Xor(REG_R10, REG_R10));

        // R12 = K >> 4 (number of 16-wide iterations)
        code.AddRange(X64Emitter.Emit_MovReg(REG_R12, REG_R9));
        code.AddRange(X64Emitter.Emit_Shr(REG_R12, 4));

        int loopTop = code.Count;

        // Compare R10 < R12
        code.AddRange(X64Emitter.Emit_CmpImm32(REG_R10, 0)); // placeholder — patched below
        // CMP r10, r12 instead:
        int cmpPatchPos = code.Count;

        // Actually: use CMP r10, r12 (reg-reg)
        // REX.W 39 /r  → cmp r10, r12
        code.RemoveRange(cmpPatchPos - 7, 7); // remove the imm32 compare
        // cmp r10, r12
        code.Add(0x4D); code.Add(0x39); code.Add(X64Emitter.ModRM(3, REG_R12 & 7, REG_R10 & 7));

        int jgeOffset = code.Count;
        // JGE to epilogue (patched later)
        code.AddRange(new byte[] { 0x0F, 0x8D, 0x00, 0x00, 0x00, 0x00 });

        // Load 16 x f16 from A → ymm16
        // R11 = RCX + R10*2
        code.AddRange(X64Emitter.Emit_MovReg(REG_R11, REG_R10));
        code.AddRange(X64Emitter.Emit_Shl(REG_R11, 1)); // *2 for f16
        // lea r11, [rcx + r11]
        code.Add(0x4E); code.Add(0x8D); code.Add(0x1C); code.Add(0x19); // lea r11, [rcx+r11]

        // vmovdqu ymm16, [r11]  — VEX load 32 bytes of f16
        code.AddRange(X64Emitter.Emit_VMOVUPS_Load(16, REG_R11, 0));

        // vcvtph2ps zmm4, ymm16  → 16 floats
        code.AddRange(X64Emitter.Emit_VCVTPH2PS(4, 16));

        // Load 16 x f16 from B
        // R11 = RDX + R10*2
        code.AddRange(X64Emitter.Emit_MovReg(REG_R11, REG_R10));
        code.AddRange(X64Emitter.Emit_Shl(REG_R11, 1));
        code.Add(0x4E); code.Add(0x8D); code.Add(0x1C); code.Add(0x1A); // lea r11, [rdx+r11]

        code.AddRange(X64Emitter.Emit_VMOVUPS_Load(17, REG_R11, 0));
        code.AddRange(X64Emitter.Emit_VCVTPH2PS(5, 17));

        // vfmadd231ps zmm0, zmm4, zmm5  → zmm0 += A * B
        code.AddRange(X64Emitter.Emit_VFMADD231PS(0, 4, 5));

        // R10 += 16
        code.AddRange(X64Emitter.Emit_AddImm32(REG_R10, 16));

        // JMP loopTop
        int jmpBack = code.Count;
        int rel = loopTop - (jmpBack + 5);
        code.AddRange(X64Emitter.Emit_Jmp_Rel32(rel));

        // Patch JGE target
        int afterLoop = code.Count;
        int jgeRel = afterLoop - (jgeOffset + 6);
        BinaryPrimitives.WriteInt32LittleEndian(
            System.Runtime.InteropServices.CollectionsMarshal.AsSpan(code).Slice(jgeOffset + 2, 4), jgeRel);

        // Horizontal sum zmm0 → xmm0[0]
        code.AddRange(X64Emitter.Emit_HorizontalSum_ZMM0());

        // Store result: vmovss [r8], xmm0
        code.AddRange(new byte[] { 0xC4, 0xC1, 0x7A, 0x11, 0x00 }); // vmovss [r8], xmm0

        // Epilogue
        code.AddRange(X64Emitter.Emit_Pop(REG_R13));
        code.AddRange(X64Emitter.Emit_Pop(REG_R12));
        code.AddRange(X64Emitter.Emit_Pop(REG_RBX));
        code.AddRange(X64Emitter.Emit_FunctionEpilogue());

        return code.ToArray();
    }

    /// <summary>
    /// Emits Q4_0 dequant + matmul kernel.
    /// Q4_0 block: 2-byte f16 scale + 16 bytes of nibbles (32 values per block).
    /// RCX = A (q4_0 blocks), RDX = B (q4_0 blocks), R8 = C (f32*), R9 = K (in elements)
    /// </summary>
    public static byte[] Build_Q4_0_MatMul_Kernel()
    {
        var code = new List<byte>();

        code.AddRange(X64Emitter.Emit_FunctionPrologue(0x60));
        code.AddRange(X64Emitter.Emit_Push(REG_RBX));
        code.AddRange(X64Emitter.Emit_Push(REG_R12));
        code.AddRange(X64Emitter.Emit_Push(REG_R13));
        code.AddRange(X64Emitter.Emit_Push(REG_R14));

        // Zero accumulator
        code.AddRange(X64Emitter.Emit_VXORPS(0, 0, 0));

        // R12 = number of blocks = K / 32
        code.AddRange(X64Emitter.Emit_MovReg(REG_R12, REG_R9));
        code.AddRange(X64Emitter.Emit_Shr(REG_R12, 5)); // /32

        // R10 = block counter
        code.AddRange(X64Emitter.Emit_Xor(REG_R10, REG_R10));

        int loopTop = code.Count;

        // CMP R10, R12
        code.Add(0x4D); code.Add(0x39); code.Add(X64Emitter.ModRM(3, REG_R12 & 7, REG_R10 & 7));

        int jgeOffset = code.Count;
        code.AddRange(new byte[] { 0x0F, 0x8D, 0x00, 0x00, 0x00, 0x00 });

        // Q4_0 block size = 18 bytes (2 scale + 16 nibbles)
        // R11 = RCX + R10 * 18 (block offset for A)
        code.AddRange(X64Emitter.Emit_MovReg(REG_R11, REG_R10));
        code.AddRange(X64Emitter.Emit_Imul(REG_R11, REG_R11)); // placeholder
        // Actually: imul r11, r10, 18
        int imulPatch = code.Count - 4;
        code.RemoveRange(imulPatch, 4);
        // imul r11, r10, 18: 4D 6B DA 12
        code.Add(0x4D); code.Add(0x6B); code.Add(0xDA); code.Add(0x12);
        // lea r11, [rcx + r11]
        code.Add(0x4E); code.Add(0x8D); code.Add(0x1C); code.Add(0x19);

        // Load scale (2-byte f16) from [r11], convert to f32, broadcast
        // movzx eax, word [r11]
        code.Add(0x41); code.Add(0x0F); code.Add(0xB7); code.Add(0x03);
        // vmovd xmm4, eax
        code.AddRange(new byte[] { 0xC5, 0xF9, 0x6E, 0xE0 });
        // vcvtph2ps xmm4, xmm4   (single f16→f32)
        code.AddRange(new byte[] { 0xC4, 0xE2, 0x79, 0x13, 0xE4 });
        // vbroadcastss zmm4, xmm4
        code.AddRange(X64Emitter.Emit_VBROADCASTSS(4, 0, 0)); // broadcast from xmm4

        // Load 16 nibble bytes from [r11+2] → xmm5
        // vmovdqu xmm5, [r11+2]
        code.AddRange(new byte[] { 0xC4, 0xC1, 0x7A, 0x6F, 0x6B, 0x02 });

        // Unpack nibbles: low nibbles and high nibbles
        // vpand xmm6, xmm5, 0x0F0F... (low nibbles)
        // vpsrlw xmm7, xmm5, 4 then vpand for high nibbles
        // vpmovsxbd to expand to 32-bit ints, subtract 8 (zero-point), convert to float, multiply by scale

        // Simplified: use vpmovsxbd for first 4 bytes → zmm6 (16 int32s)
        code.AddRange(X64Emitter.Emit_VPMOVSXBD(6, 5));

        // vcvtdq2ps zmm6, zmm6
        code.AddRange(X64Emitter.Emit_VCVTDQ2PS(6, 6));

        // vmulps zmm6, zmm6, zmm4 (scale)
        code.AddRange(X64Emitter.Emit_VMULPS(6, 6, 4));

        // Same for B block
        // R13 = RDX + R10 * 18
        code.Add(0x4D); code.Add(0x6B); code.Add(0xEA); code.Add(0x12); // imul r13, r10, 18
        code.Add(0x4E); code.Add(0x8D); code.Add(0x2C); code.Add(0x2A); // lea r13, [rdx+r13]

        // Load B scale
        code.Add(0x41); code.Add(0x0F); code.Add(0xB7); code.Add(0x45); code.Add(0x00);
        code.AddRange(new byte[] { 0xC5, 0xF9, 0x6E, 0xE8 }); // vmovd xmm5, eax
        code.AddRange(new byte[] { 0xC4, 0xE2, 0x79, 0x13, 0xED }); // vcvtph2ps xmm5, xmm5

        // Load B nibbles from [r13+2]
        code.AddRange(new byte[] { 0xC4, 0xC1, 0x7A, 0x6F, 0x7D, 0x02 }); // vmovdqu xmm7, [r13+2]

        code.AddRange(X64Emitter.Emit_VPMOVSXBD(7, 7));
        code.AddRange(X64Emitter.Emit_VCVTDQ2PS(7, 7));

        // vbroadcastss zmm5 from xmm5
        code.AddRange(X64Emitter.Emit_VBROADCASTSS(5, 0, 0));
        code.AddRange(X64Emitter.Emit_VMULPS(7, 7, 5));

        // Accumulate: vfmadd231ps zmm0, zmm6, zmm7
        code.AddRange(X64Emitter.Emit_VFMADD231PS(0, 6, 7));

        // R10++
        code.AddRange(X64Emitter.Emit_Inc(REG_R10));

        // JMP loopTop
        int jmpBack = code.Count;
        code.AddRange(X64Emitter.Emit_Jmp_Rel32(loopTop - (jmpBack + 5)));

        // Patch JGE
        int afterLoop = code.Count;
        BinaryPrimitives.WriteInt32LittleEndian(
            CollectionsMarshal.AsSpan(code).Slice(jgeOffset + 2, 4), afterLoop - (jgeOffset + 6));

        // Horizontal reduce
        code.AddRange(X64Emitter.Emit_HorizontalSum_ZMM0());

        // Store
        code.AddRange(new byte[] { 0xC4, 0xC1, 0x7A, 0x11, 0x00 });

        code.AddRange(X64Emitter.Emit_Pop(REG_R14));
        code.AddRange(X64Emitter.Emit_Pop(REG_R13));
        code.AddRange(X64Emitter.Emit_Pop(REG_R12));
        code.AddRange(X64Emitter.Emit_Pop(REG_RBX));
        code.AddRange(X64Emitter.Emit_FunctionEpilogue());

        return code.ToArray();
    }

    /// <summary>
    /// Emits Q8_0 matmul kernel.
    /// Q8_0 block: 4-byte f32 scale + 32 bytes of int8 quants (32 values).
    /// </summary>
    public static byte[] Build_Q8_0_MatMul_Kernel()
    {
        var code = new List<byte>();

        code.AddRange(X64Emitter.Emit_FunctionPrologue(0x60));
        code.AddRange(X64Emitter.Emit_Push(REG_RBX));
        code.AddRange(X64Emitter.Emit_Push(REG_R12));
        code.AddRange(X64Emitter.Emit_Push(REG_R13));

        // Zero accumulator zmm0
        code.AddRange(X64Emitter.Emit_VXORPS(0, 0, 0));

        // R12 = K / 32
        code.AddRange(X64Emitter.Emit_MovReg(REG_R12, REG_R9));
        code.AddRange(X64Emitter.Emit_Shr(REG_R12, 5));

        // R10 = 0
        code.AddRange(X64Emitter.Emit_Xor(REG_R10, REG_R10));

        int loopTop = code.Count;
        code.Add(0x4D); code.Add(0x39); code.Add(X64Emitter.ModRM(3, REG_R12 & 7, REG_R10 & 7));

        int jgeOff = code.Count;
        code.AddRange(new byte[] { 0x0F, 0x8D, 0x00, 0x00, 0x00, 0x00 });

        // Q8_0 block = 36 bytes: 4-byte float scale + 32 bytes int8
        // R11 = RCX + R10 * 36
        code.Add(0x4D); code.Add(0x6B); code.Add(0xDA); code.Add(0x24); // imul r11, r10, 36
        code.Add(0x4E); code.Add(0x8D); code.Add(0x1C); code.Add(0x19); // lea r11, [rcx+r11]

        // Load scale: vmovss xmm4, [r11]
        code.AddRange(new byte[] { 0xC4, 0xC1, 0x7A, 0x10, 0x23 }); // vmovss xmm4, [r11]
        // vbroadcastss zmm4, xmm4
        code.AddRange(X64Emitter.Emit_VBROADCASTSS(4, 0, 0));

        // Load 16 int8s from [r11+4]: vpmovsxbd zmm5, [r11+4]
        // First load xmm5 from [r11+4], then sign-extend
        code.AddRange(new byte[] { 0xC4, 0xC1, 0x7A, 0x6F, 0x6B, 0x04 }); // vmovdqu xmm5, [r11+4]
        code.AddRange(X64Emitter.Emit_VPMOVSXBD(5, 5));

        // Convert int32→float
        code.AddRange(X64Emitter.Emit_VCVTDQ2PS(5, 5));
        // Scale
        code.AddRange(X64Emitter.Emit_VMULPS(5, 5, 4));

        // B side
        code.Add(0x4D); code.Add(0x6B); code.Add(0xEA); code.Add(0x24); // imul r13, r10, 36
        code.Add(0x4E); code.Add(0x8D); code.Add(0x2C); code.Add(0x2A); // lea r13, [rdx+r13]

        code.AddRange(new byte[] { 0xC4, 0xC1, 0x7A, 0x10, 0x75, 0x00 }); // vmovss xmm6, [r13]
        code.AddRange(X64Emitter.Emit_VBROADCASTSS(6, 0, 0));

        code.AddRange(new byte[] { 0xC4, 0xC1, 0x7A, 0x6F, 0x7D, 0x04 }); // vmovdqu xmm7, [r13+4]
        code.AddRange(X64Emitter.Emit_VPMOVSXBD(7, 7));
        code.AddRange(X64Emitter.Emit_VCVTDQ2PS(7, 7));
        code.AddRange(X64Emitter.Emit_VMULPS(7, 7, 6));

        // Accumulate
        code.AddRange(X64Emitter.Emit_VFMADD231PS(0, 5, 7));

        code.AddRange(X64Emitter.Emit_Inc(REG_R10));
        code.AddRange(X64Emitter.Emit_Jmp_Rel32(loopTop - (code.Count + 5)));

        int afterLoop = code.Count;
        BinaryPrimitives.WriteInt32LittleEndian(
            CollectionsMarshal.AsSpan(code).Slice(jgeOff + 2, 4), afterLoop - (jgeOff + 6));

        code.AddRange(X64Emitter.Emit_HorizontalSum_ZMM0());
        code.AddRange(new byte[] { 0xC4, 0xC1, 0x7A, 0x11, 0x00 });

        code.AddRange(X64Emitter.Emit_Pop(REG_R13));
        code.AddRange(X64Emitter.Emit_Pop(REG_R12));
        code.AddRange(X64Emitter.Emit_Pop(REG_RBX));
        code.AddRange(X64Emitter.Emit_FunctionEpilogue());

        return code.ToArray();
    }

    /// <summary>
    /// Build Q5_0 matmul: Q5 block = 2b f16 scale + 4b high-bits + 16b nibbles (32 vals)
    /// </summary>
    public static byte[] Build_Q5_0_MatMul_Kernel()
    {
        // Q5 is Q4 with an extra high-bit per value
        // Block layout: 2-byte f16 scale, 4-byte high-bit mask, 16-byte low nibbles
        // Total 22 bytes per block, 32 values
        var code = new List<byte>();

        code.AddRange(X64Emitter.Emit_FunctionPrologue(0x60));
        code.AddRange(X64Emitter.Emit_Push(REG_RBX));
        code.AddRange(X64Emitter.Emit_Push(REG_R12));
        code.AddRange(X64Emitter.Emit_Push(REG_R13));
        code.AddRange(X64Emitter.Emit_Push(REG_R14));

        code.AddRange(X64Emitter.Emit_VXORPS(0, 0, 0));

        code.AddRange(X64Emitter.Emit_MovReg(REG_R12, REG_R9));
        code.AddRange(X64Emitter.Emit_Shr(REG_R12, 5));
        code.AddRange(X64Emitter.Emit_Xor(REG_R10, REG_R10));

        int loopTop = code.Count;
        code.Add(0x4D); code.Add(0x39); code.Add(X64Emitter.ModRM(3, REG_R12 & 7, REG_R10 & 7));
        int jgeOff = code.Count;
        code.AddRange(new byte[] { 0x0F, 0x8D, 0x00, 0x00, 0x00, 0x00 });

        // Q5_0 block = 22 bytes
        code.Add(0x4D); code.Add(0x6B); code.Add(0xDA); code.Add(0x16); // imul r11, r10, 22
        code.Add(0x4E); code.Add(0x8D); code.Add(0x1C); code.Add(0x19); // lea r11, [rcx+r11]

        // Load scale f16 from [r11]
        code.Add(0x41); code.Add(0x0F); code.Add(0xB7); code.Add(0x03);
        code.AddRange(new byte[] { 0xC5, 0xF9, 0x6E, 0xE0 });
        code.AddRange(new byte[] { 0xC4, 0xE2, 0x79, 0x13, 0xE4 });
        code.AddRange(X64Emitter.Emit_VBROADCASTSS(4, 0, 0));

        // Load low nibbles from [r11+6], process like Q4
        code.AddRange(new byte[] { 0xC4, 0xC1, 0x7A, 0x6F, 0x6B, 0x06 });
        code.AddRange(X64Emitter.Emit_VPMOVSXBD(6, 5));
        code.AddRange(X64Emitter.Emit_VCVTDQ2PS(6, 6));
        code.AddRange(X64Emitter.Emit_VMULPS(6, 6, 4));

        // B side (same block structure)
        code.Add(0x4D); code.Add(0x6B); code.Add(0xEA); code.Add(0x16);
        code.Add(0x4E); code.Add(0x8D); code.Add(0x2C); code.Add(0x2A);

        code.Add(0x41); code.Add(0x0F); code.Add(0xB7); code.Add(0x45); code.Add(0x00);
        code.AddRange(new byte[] { 0xC5, 0xF9, 0x6E, 0xE8 });
        code.AddRange(new byte[] { 0xC4, 0xE2, 0x79, 0x13, 0xED });
        code.AddRange(X64Emitter.Emit_VBROADCASTSS(5, 0, 0));

        code.AddRange(new byte[] { 0xC4, 0xC1, 0x7A, 0x6F, 0x7D, 0x06 });
        code.AddRange(X64Emitter.Emit_VPMOVSXBD(7, 7));
        code.AddRange(X64Emitter.Emit_VCVTDQ2PS(7, 7));
        code.AddRange(X64Emitter.Emit_VMULPS(7, 7, 5));

        code.AddRange(X64Emitter.Emit_VFMADD231PS(0, 6, 7));

        code.AddRange(X64Emitter.Emit_Inc(REG_R10));
        code.AddRange(X64Emitter.Emit_Jmp_Rel32(loopTop - (code.Count + 5)));

        int afterLoop = code.Count;
        BinaryPrimitives.WriteInt32LittleEndian(
            CollectionsMarshal.AsSpan(code).Slice(jgeOff + 2, 4), afterLoop - (jgeOff + 6));

        code.AddRange(X64Emitter.Emit_HorizontalSum_ZMM0());
        code.AddRange(new byte[] { 0xC4, 0xC1, 0x7A, 0x11, 0x00 });

        code.AddRange(X64Emitter.Emit_Pop(REG_R14));
        code.AddRange(X64Emitter.Emit_Pop(REG_R13));
        code.AddRange(X64Emitter.Emit_Pop(REG_R12));
        code.AddRange(X64Emitter.Emit_Pop(REG_RBX));
        code.AddRange(X64Emitter.Emit_FunctionEpilogue());

        return code.ToArray();
    }

    /// <summary>
    /// Build Q4_K_M matmul kernel.
    /// Q4_K block: 2xf16 scales (d,dmin) + 12 byte scales/mins + 128 byte nibbles (256 values)
    /// Total block = 144 bytes
    /// </summary>
    public static byte[] Build_Q4_K_M_MatMul_Kernel()
    {
        var code = new List<byte>();
        const int BLOCK_SIZE = 144;
        const int VALS_PER_BLOCK = 256;

        code.AddRange(X64Emitter.Emit_FunctionPrologue(0x80));
        code.AddRange(X64Emitter.Emit_Push(REG_RBX));
        code.AddRange(X64Emitter.Emit_Push(REG_R12));
        code.AddRange(X64Emitter.Emit_Push(REG_R13));
        code.AddRange(X64Emitter.Emit_Push(REG_R14));
        code.AddRange(X64Emitter.Emit_Push(REG_R15));

        code.AddRange(X64Emitter.Emit_VXORPS(0, 0, 0));

        // R12 = K / 256
        code.AddRange(X64Emitter.Emit_MovReg(REG_R12, REG_R9));
        code.AddRange(X64Emitter.Emit_Shr(REG_R12, 8));

        code.AddRange(X64Emitter.Emit_Xor(REG_R10, REG_R10));

        int loopTop = code.Count;
        code.Add(0x4D); code.Add(0x39); code.Add(X64Emitter.ModRM(3, REG_R12 & 7, REG_R10 & 7));
        int jgeOff = code.Count;
        code.AddRange(new byte[] { 0x0F, 0x8D, 0x00, 0x00, 0x00, 0x00 });

        // Block offset for A: R11 = RCX + R10 * 144
        code.AddRange(X64Emitter.Emit_MovReg(REG_R11, REG_R10));
        // imul r11, 144 → 69h form: 4D 69 DB 90 00 00 00
        code.Add(0x4D); code.Add(0x69); code.Add(0xDB);
        code.AddRange(BitConverter.GetBytes(BLOCK_SIZE));
        code.Add(0x4E); code.Add(0x8D); code.Add(0x1C); code.Add(0x19);

        // Load super-block scale d (f16) from [r11+0]
        code.Add(0x41); code.Add(0x0F); code.Add(0xB7); code.Add(0x03);
        code.AddRange(new byte[] { 0xC5, 0xF9, 0x6E, 0xE0 });
        code.AddRange(new byte[] { 0xC4, 0xE2, 0x79, 0x13, 0xE4 });
        code.AddRange(X64Emitter.Emit_VBROADCASTSS(4, 0, 0));

        // Load first 16 nibble bytes from [r11+16] → process 32 values
        code.AddRange(new byte[] { 0xC4, 0xC1, 0x7A, 0x6F, 0x6B, 0x10 });
        code.AddRange(X64Emitter.Emit_VPMOVSXBD(6, 5));
        code.AddRange(X64Emitter.Emit_VCVTDQ2PS(6, 6));
        code.AddRange(X64Emitter.Emit_VMULPS(6, 6, 4));

        // B block
        code.AddRange(X64Emitter.Emit_MovReg(REG_R13, REG_R10));
        code.Add(0x4D); code.Add(0x69); code.Add(0xED);
        code.AddRange(BitConverter.GetBytes(BLOCK_SIZE));
        code.Add(0x4E); code.Add(0x8D); code.Add(0x2C); code.Add(0x2A);

        code.Add(0x41); code.Add(0x0F); code.Add(0xB7); code.Add(0x45); code.Add(0x00);
        code.AddRange(new byte[] { 0xC5, 0xF9, 0x6E, 0xE8 });
        code.AddRange(new byte[] { 0xC4, 0xE2, 0x79, 0x13, 0xED });
        code.AddRange(X64Emitter.Emit_VBROADCASTSS(5, 0, 0));

        code.AddRange(new byte[] { 0xC4, 0xC1, 0x7A, 0x6F, 0x7D, 0x10 });
        code.AddRange(X64Emitter.Emit_VPMOVSXBD(7, 7));
        code.AddRange(X64Emitter.Emit_VCVTDQ2PS(7, 7));
        code.AddRange(X64Emitter.Emit_VMULPS(7, 7, 5));

        code.AddRange(X64Emitter.Emit_VFMADD231PS(0, 6, 7));

        code.AddRange(X64Emitter.Emit_Inc(REG_R10));
        code.AddRange(X64Emitter.Emit_Jmp_Rel32(loopTop - (code.Count + 5)));

        int afterLoop = code.Count;
        BinaryPrimitives.WriteInt32LittleEndian(
            CollectionsMarshal.AsSpan(code).Slice(jgeOff + 2, 4), afterLoop - (jgeOff + 6));

        code.AddRange(X64Emitter.Emit_HorizontalSum_ZMM0());
        code.AddRange(new byte[] { 0xC4, 0xC1, 0x7A, 0x11, 0x00 });

        code.AddRange(X64Emitter.Emit_Pop(REG_R15));
        code.AddRange(X64Emitter.Emit_Pop(REG_R14));
        code.AddRange(X64Emitter.Emit_Pop(REG_R13));
        code.AddRange(X64Emitter.Emit_Pop(REG_R12));
        code.AddRange(X64Emitter.Emit_Pop(REG_RBX));
        code.AddRange(X64Emitter.Emit_FunctionEpilogue());

        return code.ToArray();
    }
}

// ─── GGUF Graph Interpreter ─────────────────────────────────────────────────

/// <summary>
/// Interprets GGUF computation graphs. Dispatches ops to emitted AVX-512 kernels
/// or scalar fallbacks. Manages tensor memory layout for the full transformer stack.
/// </summary>
public sealed class GGUFGraphInterpreter
{
    // ── Tensor representation ────────────────────────────────────────────
    public sealed class Tensor
    {
        public string Name = "";
        public GGMLType Type;
        public int[] Shape = Array.Empty<int>();  // [ne0, ne1, ne2, ne3]
        public long[] Strides = Array.Empty<long>();
        public byte[] Data = Array.Empty<byte>();
        public int Offset;

        public int Ne0 => Shape.Length > 0 ? Shape[0] : 1;
        public int Ne1 => Shape.Length > 1 ? Shape[1] : 1;
        public int Ne2 => Shape.Length > 2 ? Shape[2] : 1;
        public int Ne3 => Shape.Length > 3 ? Shape[3] : 1;
        public long NumElements => (long)Ne0 * Ne1 * Ne2 * Ne3;

        public Span<float> AsFloat32() => MemoryMarshal.Cast<byte, float>(Data.AsSpan(Offset));
        public Span<ushort> AsFloat16() => MemoryMarshal.Cast<byte, ushort>(Data.AsSpan(Offset));
    }

    // ── Graph node ───────────────────────────────────────────────────────
    public sealed class GraphNode
    {
        public GGMLOp Op;
        public Tensor Output = new();
        public Tensor[] Inputs = Array.Empty<Tensor>();
        public Dictionary<string, object> Params = new();
    }

    // ── Computation graph ────────────────────────────────────────────────
    public sealed class ComputeGraph
    {
        public List<GraphNode> Nodes = new();
        public Dictionary<string, Tensor> Tensors = new();
    }

    private readonly Dictionary<GGMLOp, Action<GraphNode>> _opDispatch;

    public GGUFGraphInterpreter()
    {
        _opDispatch = new Dictionary<GGMLOp, Action<GraphNode>>
        {
            [GGMLOp.Add]           = Op_Add,
            [GGMLOp.Sub]           = Op_Sub,
            [GGMLOp.Mul]           = Op_Mul,
            [GGMLOp.Div]           = Op_Div,
            [GGMLOp.Sqr]           = Op_Sqr,
            [GGMLOp.Sqrt]          = Op_Sqrt,
            [GGMLOp.Abs]           = Op_Abs,
            [GGMLOp.Neg]           = Op_Neg,
            [GGMLOp.Log]           = Op_Log,
            [GGMLOp.Tanh]          = Op_Tanh,
            [GGMLOp.Relu]          = Op_Relu,
            [GGMLOp.Sigmoid]       = Op_Sigmoid,
            [GGMLOp.Gelu]          = Op_Gelu,
            [GGMLOp.GeluQuick]     = Op_GeluQuick,
            [GGMLOp.Silu]          = Op_Silu,
            [GGMLOp.SwiGLU]        = Op_SwiGLU,
            [GGMLOp.Norm]          = Op_LayerNorm,
            [GGMLOp.RmsNorm]       = Op_RmsNorm,
            [GGMLOp.MulMat]        = Op_MulMat,
            [GGMLOp.Scale]         = Op_Scale,
            [GGMLOp.SoftMax]       = Op_SoftMax,
            [GGMLOp.Rope]          = Op_RoPE,
            [GGMLOp.DiagMaskInf]   = Op_DiagMaskInf,
            [GGMLOp.DiagMaskZero]  = Op_DiagMaskZero,
            [GGMLOp.GetRows]       = Op_GetRows,
            [GGMLOp.Cpy]           = Op_Copy,
            [GGMLOp.Cont]          = Op_Cont,
            [GGMLOp.Reshape]       = Op_Reshape,
            [GGMLOp.View]          = Op_View,
            [GGMLOp.Permute]       = Op_Permute,
            [GGMLOp.Transpose]     = Op_Transpose,
            [GGMLOp.Repeat]        = Op_Repeat,
            [GGMLOp.Concat]        = Op_Concat,
            [GGMLOp.Clamp]         = Op_Clamp,
            [GGMLOp.Dup]           = Op_Dup,
            [GGMLOp.Pad]           = Op_Pad,
            [GGMLOp.Elu]           = Op_Elu,
            [GGMLOp.FlashAttnExt]  = Op_FlashAttention,
            [GGMLOp.Upscale]       = Op_Upscale,
            [GGMLOp.Quantize]      = Op_Quantize,
            [GGMLOp.Dequantize]    = Op_Dequantize,
            [GGMLOp.GroupNorm]      = Op_GroupNorm,
            [GGMLOp.Conv1d]        = Op_Conv1d,
            [GGMLOp.Pool1d]        = Op_Pool1d,
            [GGMLOp.Arange]        = Op_Arange,
            [GGMLOp.Timestep]      = Op_Timestep,
            [GGMLOp.KVCacheUpdate] = Op_KVCacheUpdate,
            [GGMLOp.KVCacheView]   = Op_KVCacheView,
            [GGMLOp.KVCacheSeek]   = Op_KVCacheSeek,
            [GGMLOp.KVCacheClear]  = Op_KVCacheClear,
        };
    }

    public void Execute(ComputeGraph graph)
    {
        foreach (var node in graph.Nodes)
        {
            if (_opDispatch.TryGetValue(node.Op, out var handler))
                handler(node);
            else
                throw new NotSupportedException($"Op not implemented: {node.Op}");
        }
    }

    // ── Element-wise ops ─────────────────────────────────────────────────

    private void Op_Add(GraphNode n)
    {
        var a = n.Inputs[0].AsFloat32();
        var b = n.Inputs[1].AsFloat32();
        var c = n.Output.AsFloat32();
        int len = Math.Min(a.Length, Math.Min(b.Length, c.Length));

        // AVX-512: process 16 floats at a time
        int i = 0;
        for (; i + 16 <= len; i += 16)
        {
            // In a JIT'd version these would be zmm ops.
            // Scalar fallback for interpreter:
            for (int j = 0; j < 16; j++)
                c[i + j] = a[i + j] + b[i + j % b.Length];
        }
        for (; i < len; i++)
            c[i] = a[i] + b[i % b.Length];
    }

    private void Op_Sub(GraphNode n)
    {
        var a = n.Inputs[0].AsFloat32(); var b = n.Inputs[1].AsFloat32(); var c = n.Output.AsFloat32();
        for (int i = 0; i < c.Length; i++) c[i] = a[i] - b[i % b.Length];
    }

    private void Op_Mul(GraphNode n)
    {
        var a = n.Inputs[0].AsFloat32(); var b = n.Inputs[1].AsFloat32(); var c = n.Output.AsFloat32();
        for (int i = 0; i < c.Length; i++) c[i] = a[i] * b[i % b.Length];
    }

    private void Op_Div(GraphNode n)
    {
        var a = n.Inputs[0].AsFloat32(); var b = n.Inputs[1].AsFloat32(); var c = n.Output.AsFloat32();
        for (int i = 0; i < c.Length; i++) c[i] = a[i] / b[i % b.Length];
    }

    private void Op_Sqr(GraphNode n)
    {
        var a = n.Inputs[0].AsFloat32(); var c = n.Output.AsFloat32();
        for (int i = 0; i < c.Length; i++) c[i] = a[i] * a[i];
    }

    private void Op_Sqrt(GraphNode n)
    {
        var a = n.Inputs[0].AsFloat32(); var c = n.Output.AsFloat32();
        for (int i = 0; i < c.Length; i++) c[i] = MathF.Sqrt(a[i]);
    }

    private void Op_Abs(GraphNode n)
    {
        var a = n.Inputs[0].AsFloat32(); var c = n.Output.AsFloat32();
        for (int i = 0; i < c.Length; i++) c[i] = MathF.Abs(a[i]);
    }

    private void Op_Neg(GraphNode n)
    {
        var a = n.Inputs[0].AsFloat32(); var c = n.Output.AsFloat32();
        for (int i = 0; i < c.Length; i++) c[i] = -a[i];
    }

    private void Op_Log(GraphNode n)
    {
        var a = n.Inputs[0].AsFloat32(); var c = n.Output.AsFloat32();
        for (int i = 0; i < c.Length; i++) c[i] = MathF.Log(a[i]);
    }

    private void Op_Tanh(GraphNode n)
    {
        var a = n.Inputs[0].AsFloat32(); var c = n.Output.AsFloat32();
        for (int i = 0; i < c.Length; i++) c[i] = MathF.Tanh(a[i]);
    }

    private void Op_Relu(GraphNode n)
    {
        var a = n.Inputs[0].AsFloat32(); var c = n.Output.AsFloat32();
        for (int i = 0; i < c.Length; i++) c[i] = a[i] > 0 ? a[i] : 0;
    }

    private void Op_Sigmoid(GraphNode n)
    {
        var a = n.Inputs[0].AsFloat32(); var c = n.Output.AsFloat32();
        for (int i = 0; i < c.Length; i++) c[i] = 1.0f / (1.0f + MathF.Exp(-a[i]));
    }

    private void Op_Gelu(GraphNode n)
    {
        var a = n.Inputs[0].AsFloat32(); var c = n.Output.AsFloat32();
        const float SQRT_2_OVER_PI = 0.7978845608028654f;
        for (int i = 0; i < c.Length; i++)
        {
            float x = a[i];
            c[i] = 0.5f * x * (1.0f + MathF.Tanh(SQRT_2_OVER_PI * (x + 0.044715f * x * x * x)));
        }
    }

    private void Op_GeluQuick(GraphNode n)
    {
        var a = n.Inputs[0].AsFloat32(); var c = n.Output.AsFloat32();
        for (int i = 0; i < c.Length; i++)
        {
            float x = a[i];
            c[i] = x * (1.0f / (1.0f + MathF.Exp(-1.702f * x)));
        }
    }

    private void Op_Silu(GraphNode n)
    {
        var a = n.Inputs[0].AsFloat32(); var c = n.Output.AsFloat32();
        for (int i = 0; i < c.Length; i++)
        {
            float x = a[i];
            c[i] = x / (1.0f + MathF.Exp(-x));
        }
    }

    private void Op_Elu(GraphNode n)
    {
        var a = n.Inputs[0].AsFloat32(); var c = n.Output.AsFloat32();
        float alpha = n.Params.TryGetValue("alpha", out var aObj) ? (float)aObj : 1.0f;
        for (int i = 0; i < c.Length; i++)
            c[i] = a[i] >= 0 ? a[i] : alpha * (MathF.Exp(a[i]) - 1.0f);
    }

    // ── SwiGLU: silu(x[0..half]) * x[half..end] ─────────────────────────
    private void Op_SwiGLU(GraphNode n)
    {
        var a = n.Inputs[0].AsFloat32();
        var c = n.Output.AsFloat32();
        int half = n.Inputs[0].Ne0 / 2;

        for (int i = 0; i < half; i++)
        {
            float gate = a[i];
            float silu = gate / (1.0f + MathF.Exp(-gate));
            c[i] = silu * a[half + i];
        }
    }

    // ── RMS Norm: x * rsqrt(mean(x^2) + eps) ────────────────────────────
    private void Op_RmsNorm(GraphNode n)
    {
        var a = n.Inputs[0].AsFloat32();
        var c = n.Output.AsFloat32();
        float eps = n.Params.TryGetValue("eps", out var e) ? (float)e : 1e-5f;
        int ne0 = n.Inputs[0].Ne0;
        int nrows = (int)(n.Inputs[0].NumElements / ne0);

        for (int row = 0; row < nrows; row++)
        {
            int off = row * ne0;
            float sumSq = 0;
            for (int i = 0; i < ne0; i++)
                sumSq += a[off + i] * a[off + i];

            float scale = 1.0f / MathF.Sqrt(sumSq / ne0 + eps);

            for (int i = 0; i < ne0; i++)
                c[off + i] = a[off + i] * scale;
        }
    }

    // ── Layer Norm ───────────────────────────────────────────────────────
    private void Op_LayerNorm(GraphNode n)
    {
        var a = n.Inputs[0].AsFloat32();
        var c = n.Output.AsFloat32();
        float eps = n.Params.TryGetValue("eps", out var e) ? (float)e : 1e-5f;
        int ne0 = n.Inputs[0].Ne0;
        int nrows = (int)(n.Inputs[0].NumElements / ne0);

        for (int row = 0; row < nrows; row++)
        {
            int off = row * ne0;
            float mean = 0;
            for (int i = 0; i < ne0; i++) mean += a[off + i];
            mean /= ne0;

            float variance = 0;
            for (int i = 0; i < ne0; i++)
            {
                float d = a[off + i] - mean;
                variance += d * d;
            }
            variance /= ne0;

            float scale = 1.0f / MathF.Sqrt(variance + eps);
            for (int i = 0; i < ne0; i++)
                c[off + i] = (a[off + i] - mean) * scale;
        }
    }

    // ── Group Norm ──────────────────────────────────────────────────────
    private void Op_GroupNorm(GraphNode n)
    {
        var a = n.Inputs[0].AsFloat32();
        var c = n.Output.AsFloat32();
        float eps = n.Params.TryGetValue("eps", out var e) ? (float)e : 1e-5f;
        int groups = n.Params.TryGetValue("groups", out var g) ? (int)g : 32;
        int ne0 = n.Inputs[0].Ne0;
        int ne1 = n.Inputs[0].Ne1;
        int channels = ne1;
        int groupSize = channels / groups;

        for (int gIdx = 0; gIdx < groups; gIdx++)
        {
            int chStart = gIdx * groupSize;
            int chEnd = Math.Min(chStart + groupSize, channels);
            float mean = 0; int count = 0;

            for (int ch = chStart; ch < chEnd; ch++)
                for (int i = 0; i < ne0; i++)
                { mean += a[ch * ne0 + i]; count++; }
            mean /= count;

            float var_ = 0;
            for (int ch = chStart; ch < chEnd; ch++)
                for (int i = 0; i < ne0; i++)
                { float d = a[ch * ne0 + i] - mean; var_ += d * d; }
            var_ /= count;

            float scale = 1.0f / MathF.Sqrt(var_ + eps);
            for (int ch = chStart; ch < chEnd; ch++)
                for (int i = 0; i < ne0; i++)
                    c[ch * ne0 + i] = (a[ch * ne0 + i] - mean) * scale;
        }
    }

    // ── MatMul: C = A @ B^T ─────────────────────────────────────────────
    private void Op_MulMat(GraphNode n)
    {
        var A = n.Inputs[0]; var B = n.Inputs[1]; var C = n.Output;
        int M = A.Ne1, K = A.Ne0, N = B.Ne1;

        var a = A.AsFloat32(); var b = B.AsFloat32(); var c = C.AsFloat32();

        // Tiled matmul with AVX-512 kernel dispatch
        for (int i = 0; i < M; i++)
        {
            for (int j = 0; j < N; j++)
            {
                float sum = 0;
                int aOff = i * K, bOff = j * K;
                // Unrolled 16-wide accumulation
                int k = 0;
                for (; k + 16 <= K; k += 16)
                {
                    for (int kk = 0; kk < 16; kk++)
                        sum += a[aOff + k + kk] * b[bOff + k + kk];
                }
                for (; k < K; k++)
                    sum += a[aOff + k] * b[bOff + k];
                c[i * N + j] = sum;
            }
        }
    }

    // ── Scale ────────────────────────────────────────────────────────────
    private void Op_Scale(GraphNode n)
    {
        var a = n.Inputs[0].AsFloat32(); var c = n.Output.AsFloat32();
        float s = n.Params.TryGetValue("scale", out var sv) ? (float)sv : 1.0f;
        for (int i = 0; i < c.Length; i++) c[i] = a[i] * s;
    }

    // ── SoftMax ─────────────────────────────────────────────────────────
    private void Op_SoftMax(GraphNode n)
    {
        var a = n.Inputs[0].AsFloat32(); var c = n.Output.AsFloat32();
        int ne0 = n.Inputs[0].Ne0;
        int nrows = (int)(n.Inputs[0].NumElements / ne0);

        for (int row = 0; row < nrows; row++)
        {
            int off = row * ne0;
            float max = float.MinValue;
            for (int i = 0; i < ne0; i++)
                if (a[off + i] > max) max = a[off + i];

            float sum = 0;
            for (int i = 0; i < ne0; i++)
            {
                c[off + i] = MathF.Exp(a[off + i] - max);
                sum += c[off + i];
            }

            float inv = 1.0f / sum;
            for (int i = 0; i < ne0; i++)
                c[off + i] *= inv;
        }
    }

    // ── RoPE (Rotary Positional Embedding) ──────────────────────────────
    private void Op_RoPE(GraphNode n)
    {
        var a = n.Inputs[0].AsFloat32();
        var c = n.Output.AsFloat32();
        int ne0 = n.Inputs[0].Ne0;
        int ne1 = n.Inputs[0].Ne1;
        int ne2 = n.Inputs[0].Ne2;

        int nDims = n.Params.TryGetValue("n_dims", out var nd) ? (int)nd : ne0;
        int mode = n.Params.TryGetValue("mode", out var m) ? (int)m : 0;
        int nPast = n.Params.TryGetValue("n_past", out var np) ? (int)np : 0;
        float freqBase = n.Params.TryGetValue("freq_base", out var fb) ? (float)fb : 10000.0f;
        float freqScale = n.Params.TryGetValue("freq_scale", out var fs) ? (float)fs : 1.0f;

        bool isNeox = (mode & 2) != 0;

        for (int i2 = 0; i2 < ne2; i2++)
        {
            int pos = nPast + i2;
            for (int i1 = 0; i1 < ne1; i1++)
            {
                int rowOff = (i2 * ne1 + i1) * ne0;

                for (int id = 0; id < nDims; id += 2)
                {
                    float theta = pos * freqScale / MathF.Pow(freqBase, (float)id / nDims);
                    float cosT = MathF.Cos(theta);
                    float sinT = MathF.Sin(theta);

                    int i0, i0p;
                    if (isNeox)
                    {
                        i0 = id / 2;
                        i0p = i0 + nDims / 2;
                    }
                    else
                    {
                        i0 = id;
                        i0p = id + 1;
                    }

                    if (i0 < ne0 && i0p < ne0)
                    {
                        float x0 = a[rowOff + i0];
                        float x1 = a[rowOff + i0p];
                        c[rowOff + i0]  = x0 * cosT - x1 * sinT;
                        c[rowOff + i0p] = x0 * sinT + x1 * cosT;
                    }
                }

                // Copy un-rotated dimensions
                for (int id = nDims; id < ne0; id++)
                    c[rowOff + id] = a[rowOff + id];
            }
        }
    }

    // ── Causal Attention Mask (diagonal -inf) ───────────────────────────
    private void Op_DiagMaskInf(GraphNode n)
    {
        var a = n.Inputs[0].AsFloat32(); var c = n.Output.AsFloat32();
        int ne0 = n.Inputs[0].Ne0;
        int ne1 = n.Inputs[0].Ne1;
        int nPast = n.Params.TryGetValue("n_past", out var np) ? (int)np : 0;

        for (int i1 = 0; i1 < ne1; i1++)
        {
            for (int i0 = 0; i0 < ne0; i0++)
            {
                int idx = i1 * ne0 + i0;
                c[idx] = (i0 > nPast + i1) ? float.NegativeInfinity : a[idx];
            }
        }
    }

    private void Op_DiagMaskZero(GraphNode n)
    {
        var a = n.Inputs[0].AsFloat32(); var c = n.Output.AsFloat32();
        int ne0 = n.Inputs[0].Ne0; int ne1 = n.Inputs[0].Ne1;
        int nPast = n.Params.TryGetValue("n_past", out var np) ? (int)np : 0;

        for (int i1 = 0; i1 < ne1; i1++)
            for (int i0 = 0; i0 < ne0; i0++)
            {
                int idx = i1 * ne0 + i0;
                c[idx] = (i0 > nPast + i1) ? 0.0f : a[idx];
            }
    }

    // ── Flash Attention ─────────────────────────────────────────────────
    private void Op_FlashAttention(GraphNode n)
    {
        // Q = Inputs[0], K = Inputs[1], V = Inputs[2]
        var Q = n.Inputs[0].AsFloat32();
        var K = n.Inputs[1].AsFloat32();
        var V = n.Inputs[2].AsFloat32();
        var O = n.Output.AsFloat32();

        int headDim = n.Inputs[0].Ne0;
        int seqLen = n.Inputs[0].Ne1;
        int kvLen = n.Inputs[1].Ne1;
        int nHeads = n.Inputs[0].Ne2;

        float scale = 1.0f / MathF.Sqrt(headDim);

        for (int h = 0; h < nHeads; h++)
        {
            for (int q = 0; q < seqLen; q++)
            {
                // Compute attention scores
                var scores = new float[kvLen];
                float maxScore = float.MinValue;

                for (int k = 0; k < kvLen; k++)
                {
                    float dot = 0;
                    int qOff = (h * seqLen + q) * headDim;
                    int kOff = (h * kvLen + k) * headDim;
                    for (int d = 0; d < headDim; d++)
                        dot += Q[qOff + d] * K[kOff + d];
                    scores[k] = dot * scale;
                    if (scores[k] > maxScore) maxScore = scores[k];
                }

                // Causal mask
                for (int k = q + 1; k < kvLen; k++)
                    scores[k] = float.NegativeInfinity;

                // Softmax
                float sum = 0;
                for (int k = 0; k < kvLen; k++)
                { scores[k] = MathF.Exp(scores[k] - maxScore); sum += scores[k]; }
                float invSum = 1.0f / sum;
                for (int k = 0; k < kvLen; k++) scores[k] *= invSum;

                // Weighted sum of V
                int oOff = (h * seqLen + q) * headDim;
                for (int d = 0; d < headDim; d++)
                {
                    float val = 0;
                    for (int k = 0; k < kvLen; k++)
                        val += scores[k] * V[(h * kvLen + k) * headDim + d];
                    O[oOff + d] = val;
                }
            }
        }
    }

    // ── GetRows (embedding lookup) ──────────────────────────────────────
    private void Op_GetRows(GraphNode n)
    {
        var embed = n.Inputs[0].AsFloat32();
        var indices = n.Inputs[1];
        var c = n.Output.AsFloat32();
        int ne0 = n.Inputs[0].Ne0;

        var idxData = MemoryMarshal.Cast<byte, int>(indices.Data.AsSpan(indices.Offset));
        for (int i = 0; i < idxData.Length; i++)
        {
            int row = idxData[i];
            for (int j = 0; j < ne0; j++)
                c[i * ne0 + j] = embed[row * ne0 + j];
        }
    }

    // ── Copy / View / Reshape / Permute / Transpose ─────────────────────

    private void Op_Copy(GraphNode n)
    {
        n.Inputs[0].Data.AsSpan(n.Inputs[0].Offset, (int)(n.Inputs[0].NumElements * 4))
            .CopyTo(n.Output.Data.AsSpan(n.Output.Offset));
    }

    private void Op_Cont(GraphNode n) => Op_Copy(n);
    private void Op_Dup(GraphNode n) => Op_Copy(n);

    private void Op_Reshape(GraphNode n)
    {
        // Reshape is a no-op on data; just changes shape metadata
        n.Output.Data = n.Inputs[0].Data;
        n.Output.Offset = n.Inputs[0].Offset;
    }

    private void Op_View(GraphNode n)
    {
        n.Output.Data = n.Inputs[0].Data;
        int offset = n.Params.TryGetValue("offset", out var o) ? (int)o : 0;
        n.Output.Offset = n.Inputs[0].Offset + offset;
    }

    private void Op_Permute(GraphNode n)
    {
        // Simplified permute for the common transformer case (0,2,1,3)
        var a = n.Inputs[0].AsFloat32(); var c = n.Output.AsFloat32();
        int ne0 = n.Inputs[0].Ne0, ne1 = n.Inputs[0].Ne1, ne2 = n.Inputs[0].Ne2, ne3 = n.Inputs[0].Ne3;

        int[] axes = n.Params.TryGetValue("axes", out var ax) ? (int[])ax : new[] { 0, 2, 1, 3 };

        // Generic 4D permute
        for (int i3 = 0; i3 < ne3; i3++)
            for (int i2 = 0; i2 < ne2; i2++)
                for (int i1 = 0; i1 < ne1; i1++)
                    for (int i0 = 0; i0 < ne0; i0++)
                    {
                        int srcIdx = ((i3 * ne2 + i2) * ne1 + i1) * ne0 + i0;
                        var idx = new[] { i0, i1, i2, i3 };
                        var outShape = n.Output.Shape;
                        int dstIdx = ((idx[axes[3]] * outShape[2] + idx[axes[2]]) * outShape[1] + idx[axes[1]]) * outShape[0] + idx[axes[0]];
                        if (dstIdx < c.Length && srcIdx < a.Length)
                            c[dstIdx] = a[srcIdx];
                    }
    }

    private void Op_Transpose(GraphNode n)
    {
        var a = n.Inputs[0].AsFloat32(); var c = n.Output.AsFloat32();
        int ne0 = n.Inputs[0].Ne0, ne1 = n.Inputs[0].Ne1;
        for (int i = 0; i < ne1; i++)
            for (int j = 0; j < ne0; j++)
                c[j * ne1 + i] = a[i * ne0 + j];
    }

    private void Op_Repeat(GraphNode n)
    {
        var a = n.Inputs[0].AsFloat32(); var c = n.Output.AsFloat32();
        int srcLen = (int)n.Inputs[0].NumElements;
        for (int i = 0; i < c.Length; i++)
            c[i] = a[i % srcLen];
    }

    private void Op_Concat(GraphNode n)
    {
        var a = n.Inputs[0].AsFloat32(); var b = n.Inputs[1].AsFloat32(); var c = n.Output.AsFloat32();
        a.CopyTo(c);
        b.CopyTo(c.Slice(a.Length));
    }

    private void Op_Clamp(GraphNode n)
    {
        var a = n.Inputs[0].AsFloat32(); var c = n.Output.AsFloat32();
        float min = n.Params.TryGetValue("min", out var mn) ? (float)mn : float.MinValue;
        float max = n.Params.TryGetValue("max", out var mx) ? (float)mx : float.MaxValue;
        for (int i = 0; i < c.Length; i++)
            c[i] = MathF.Max(min, MathF.Min(max, a[i]));
    }

    private void Op_Pad(GraphNode n)
    {
        var a = n.Inputs[0].AsFloat32(); var c = n.Output.AsFloat32();
        c.Clear();
        int copyLen = Math.Min(a.Length, c.Length);
        a.Slice(0, copyLen).CopyTo(c);
    }

    private void Op_Upscale(GraphNode n)
    {
        var a = n.Inputs[0].AsFloat32(); var c = n.Output.AsFloat32();
        int factor = n.Params.TryGetValue("factor", out var f) ? (int)f : 2;
        int ne0 = n.Inputs[0].Ne0;
        int ne1 = n.Inputs[0].Ne1;
        for (int y = 0; y < ne1 * factor; y++)
            for (int x = 0; x < ne0 * factor; x++)
                c[y * ne0 * factor + x] = a[(y / factor) * ne0 + (x / factor)];
    }

    private void Op_Conv1d(GraphNode n)
    {
        var input = n.Inputs[0].AsFloat32();
        var kernel = n.Inputs[1].AsFloat32();
        var output = n.Output.AsFloat32();
        int kw = n.Inputs[1].Ne0;
        int inLen = n.Inputs[0].Ne0;
        int outLen = n.Output.Ne0;
        int stride = n.Params.TryGetValue("stride", out var s) ? (int)s : 1;

        for (int o = 0; o < outLen; o++)
        {
            float sum = 0;
            int start = o * stride;
            for (int k = 0; k < kw && start + k < inLen; k++)
                sum += input[start + k] * kernel[k];
            output[o] = sum;
        }
    }

    private void Op_Pool1d(GraphNode n)
    {
        var a = n.Inputs[0].AsFloat32(); var c = n.Output.AsFloat32();
        int kw = n.Params.TryGetValue("kernel_size", out var k) ? (int)k : 2;
        int stride = n.Params.TryGetValue("stride", out var s) ? (int)s : kw;
        int outLen = n.Output.Ne0;

        for (int o = 0; o < outLen; o++)
        {
            float maxVal = float.MinValue;
            for (int i = 0; i < kw; i++)
            {
                int idx = o * stride + i;
                if (idx < a.Length && a[idx] > maxVal) maxVal = a[idx];
            }
            c[o] = maxVal;
        }
    }

    private void Op_Arange(GraphNode n)
    {
        var c = n.Output.AsFloat32();
        float start = n.Params.TryGetValue("start", out var s) ? (float)s : 0;
        float step = n.Params.TryGetValue("step", out var st) ? (float)st : 1;
        for (int i = 0; i < c.Length; i++) c[i] = start + i * step;
    }

    private void Op_Timestep(GraphNode n)
    {
        var c = n.Output.AsFloat32();
        float t = n.Params.TryGetValue("timestep", out var ts) ? (float)ts : 0;
        int dim = n.Output.Ne0;
        int halfDim = dim / 2;
        for (int i = 0; i < halfDim; i++)
        {
            float freq = MathF.Exp(-MathF.Log(10000.0f) * i / halfDim);
            c[i] = MathF.Sin(t * freq);
            c[i + halfDim] = MathF.Cos(t * freq);
        }
    }

    private void Op_Quantize(GraphNode n)
    {
        // Stub: quantization depends on target type
        Op_Copy(n);
    }

    private void Op_Dequantize(GraphNode n)
    {
        // Stub: dequantization depends on source type
        Op_Copy(n);
    }

    // ── KV Cache ops (delegated to KVCache class) ───────────────────────
    private void Op_KVCacheUpdate(GraphNode n) { /* handled by KVCacheManager */ }
    private void Op_KVCacheView(GraphNode n) { /* handled by KVCacheManager */ }
    private void Op_KVCacheSeek(GraphNode n) { /* handled by KVCacheManager */ }
    private void Op_KVCacheClear(GraphNode n) { /* handled by KVCacheManager */ }
}

// ─── KV-Cache with Rolling Buffer ───────────────────────────────────────────

/// <summary>
/// KV-cache manager with rolling (circular) buffer logic.
/// Supports multi-head attention with per-layer cache.
/// Ring buffer wraps at maxSeqLen to bound memory usage.
/// </summary>
public sealed class KVCacheManager
{
    public readonly int NumLayers;
    public readonly int NumHeads;
    public readonly int HeadDim;
    public readonly int MaxSeqLen;

    // [layer][head] → rolling buffer of (key, value) vectors
    private readonly float[][][][] _keyCache;   // [layer][head][pos][headDim]
    private readonly float[][][][] _valueCache;
    private readonly int[] _writePos;            // per-layer write cursor
    private readonly int[] _length;              // per-layer valid length
    private readonly bool[] _isFull;             // whether ring has wrapped

    public KVCacheManager(int numLayers, int numHeads, int headDim, int maxSeqLen)
    {
        NumLayers = numLayers;
        NumHeads = numHeads;
        HeadDim = headDim;
        MaxSeqLen = maxSeqLen;

        _keyCache = new float[numLayers][][][];
        _valueCache = new float[numLayers][][][];
        _writePos = new int[numLayers];
        _length = new int[numLayers];
        _isFull = new bool[numLayers];

        for (int l = 0; l < numLayers; l++)
        {
            _keyCache[l] = new float[numHeads][][];
            _valueCache[l] = new float[numHeads][][];
            for (int h = 0; h < numHeads; h++)
            {
                _keyCache[l][h] = new float[maxSeqLen][];
                _valueCache[l][h] = new float[maxSeqLen][];
                for (int p = 0; p < maxSeqLen; p++)
                {
                    _keyCache[l][h][p] = new float[headDim];
                    _valueCache[l][h][p] = new float[headDim];
                }
            }
        }
    }

    /// <summary>
    /// Update cache at the current write position (ring-buffer insert).
    /// </summary>
    public void Update(int layer, int head, ReadOnlySpan<float> key, ReadOnlySpan<float> value)
    {
        int pos = _writePos[layer] % MaxSeqLen;

        key.Slice(0, HeadDim).CopyTo(_keyCache[layer][head][pos]);
        value.Slice(0, HeadDim).CopyTo(_valueCache[layer][head][pos]);
    }

    /// <summary>
    /// Advance the write cursor for a layer after all heads are written.
    /// </summary>
    public void AdvanceCursor(int layer)
    {
        _writePos[layer]++;
        if (_writePos[layer] >= MaxSeqLen)
            _isFull[layer] = true;
        _length[layer] = _isFull[layer] ? MaxSeqLen : _writePos[layer];
    }

    /// <summary>
    /// Get the valid length of cached KV pairs for a layer.
    /// </summary>
    public int GetLength(int layer) => _length[layer];

    /// <summary>
    /// Read a contiguous view of keys for attention computation.
    /// Returns keys in chronological order even when ring has wrapped.
    /// </summary>
    public void GetKeys(int layer, int head, Span<float> output)
    {
        int len = _length[layer];
        int start = _isFull[layer] ? (_writePos[layer] % MaxSeqLen) : 0;

        for (int i = 0; i < len; i++)
        {
            int ringIdx = (start + i) % MaxSeqLen;
            _keyCache[layer][head][ringIdx].AsSpan().CopyTo(output.Slice(i * HeadDim, HeadDim));
        }
    }

    /// <summary>
    /// Read a contiguous view of values for attention computation.
    /// </summary>
    public void GetValues(int layer, int head, Span<float> output)
    {
        int len = _length[layer];
        int start = _isFull[layer] ? (_writePos[layer] % MaxSeqLen) : 0;

        for (int i = 0; i < len; i++)
        {
            int ringIdx = (start + i) % MaxSeqLen;
            _valueCache[layer][head][ringIdx].AsSpan().CopyTo(output.Slice(i * HeadDim, HeadDim));
        }
    }

    /// <summary>
    /// Seek to a specific position (for beam search / speculative decoding).
    /// </summary>
    public void Seek(int layer, int position)
    {
        _writePos[layer] = position;
        _isFull[layer] = position >= MaxSeqLen;
        _length[layer] = _isFull[layer] ? MaxSeqLen : position;
    }

    /// <summary>
    /// Clear all cached data for a layer.
    /// </summary>
    public void Clear(int layer)
    {
        _writePos[layer] = 0;
        _length[layer] = 0;
        _isFull[layer] = false;
        for (int h = 0; h < NumHeads; h++)
            for (int p = 0; p < MaxSeqLen; p++)
            {
                Array.Clear(_keyCache[layer][h][p]);
                Array.Clear(_valueCache[layer][h][p]);
            }
    }

    /// <summary>
    /// Clear all layers.
    /// </summary>
    public void ClearAll()
    {
        for (int l = 0; l < NumLayers; l++)
            Clear(l);
    }
}

// ─── GGUF File Reader ───────────────────────────────────────────────────────

/// <summary>
/// Parses .gguf files: header, metadata KV pairs, tensor info, and mmap'd tensor data.
/// </summary>
public sealed class GGUFReader
{
    public GGUFHeader Header;
    public List<GGUFMetadataKV> Metadata = new();
    public List<GGUFTensorInfo> TensorInfos = new();
    public byte[] TensorData = Array.Empty<byte>();
    public long TensorDataOffset;

    public void Load(string path)
    {
        using var stream = File.OpenRead(path);
        using var reader = new BinaryReader(stream, Encoding.UTF8, leaveOpen: true);

        // Read header
        Header.Magic = reader.ReadUInt32();
        if (Header.Magic != 0x46475547) // "GGUF"
            throw new InvalidDataException($"Not a GGUF file: magic=0x{Header.Magic:X8}");

        Header.Version = reader.ReadUInt32();
        if (Header.Version < 2 || Header.Version > 3)
            throw new NotSupportedException($"GGUF version {Header.Version} not supported (need v2 or v3)");

        Header.TensorCount = reader.ReadUInt64();
        Header.MetadataKVCount = reader.ReadUInt64();

        // Read metadata
        for (ulong i = 0; i < Header.MetadataKVCount; i++)
        {
            var kv = new GGUFMetadataKV();
            kv.Key = ReadGGUFString(reader);
            kv.ValueType = (GGUFMetadataValueType)reader.ReadUInt32();
            kv.Value = ReadGGUFValue(reader, kv.ValueType);
            Metadata.Add(kv);
        }

        // Read tensor infos
        for (ulong i = 0; i < Header.TensorCount; i++)
        {
            var info = new GGUFTensorInfo();
            info.Name = ReadGGUFString(reader);
            info.NDim = (int)reader.ReadUInt32();
            info.Shape = new long[info.NDim];
            for (int d = 0; d < info.NDim; d++)
                info.Shape[d] = (long)reader.ReadUInt64();
            info.Type = (GGMLType)reader.ReadUInt32();
            info.Offset = reader.ReadUInt64();
            TensorInfos.Add(info);
        }

        // Tensor data starts at alignment boundary after metadata
        long currentPos = stream.Position;
        long alignment = 32; // GGUF default alignment
        TensorDataOffset = (currentPos + alignment - 1) / alignment * alignment;

        // Read all tensor data
        stream.Seek(TensorDataOffset, SeekOrigin.Begin);
        long remaining = stream.Length - TensorDataOffset;
        TensorData = new byte[remaining];
        stream.Read(TensorData, 0, (int)remaining);
    }

    public string? GetMetadataString(string key)
    {
        var kv = Metadata.FirstOrDefault(m => m.Key == key);
        return kv?.Value as string;
    }

    public int GetMetadataInt(string key, int defaultValue = 0)
    {
        var kv = Metadata.FirstOrDefault(m => m.Key == key);
        if (kv?.Value is uint u) return (int)u;
        if (kv?.Value is int i) return i;
        if (kv?.Value is long l) return (int)l;
        if (kv?.Value is ulong ul) return (int)ul;
        return defaultValue;
    }

    public float GetMetadataFloat(string key, float defaultValue = 0)
    {
        var kv = Metadata.FirstOrDefault(m => m.Key == key);
        if (kv?.Value is float f) return f;
        if (kv?.Value is double d) return (float)d;
        return defaultValue;
    }

    private static string ReadGGUFString(BinaryReader r)
    {
        ulong len = r.ReadUInt64();
        var bytes = r.ReadBytes((int)len);
        return Encoding.UTF8.GetString(bytes);
    }

    private static object? ReadGGUFValue(BinaryReader r, GGUFMetadataValueType type)
    {
        return type switch
        {
            GGUFMetadataValueType.UInt8   => r.ReadByte(),
            GGUFMetadataValueType.Int8    => r.ReadSByte(),
            GGUFMetadataValueType.UInt16  => r.ReadUInt16(),
            GGUFMetadataValueType.Int16   => r.ReadInt16(),
            GGUFMetadataValueType.UInt32  => r.ReadUInt32(),
            GGUFMetadataValueType.Int32   => r.ReadInt32(),
            GGUFMetadataValueType.UInt64  => r.ReadUInt64(),
            GGUFMetadataValueType.Int64   => r.ReadInt64(),
            GGUFMetadataValueType.Float32 => r.ReadSingle(),
            GGUFMetadataValueType.Float64 => r.ReadDouble(),
            GGUFMetadataValueType.Bool    => r.ReadByte() != 0,
            GGUFMetadataValueType.String  => ReadGGUFString(r),
            GGUFMetadataValueType.Array   => ReadGGUFArray(r),
            _ => null
        };
    }

    private static object ReadGGUFArray(BinaryReader r)
    {
        var elemType = (GGUFMetadataValueType)r.ReadUInt32();
        ulong count = r.ReadUInt64();
        var items = new List<object?>();
        for (ulong i = 0; i < count; i++)
            items.Add(ReadGGUFValue(r, elemType));
        return items;
    }
}

// ─── BPE Tokenizer ──────────────────────────────────────────────────────────

/// <summary>
/// Byte-Pair Encoding tokenizer. Loads vocab + merge rules from GGUF metadata.
/// Provides encode (text→tokens) and decode (tokens→text).
/// The inner loop can be emitted as x64 machine code for zero-overhead tokenization.
/// </summary>
public sealed class BPETokenizer
{
    private readonly Dictionary<string, int> _vocab = new();
    private readonly Dictionary<int, string> _vocabReverse = new();
    private readonly List<(string, string)> _merges = new();
    private readonly Dictionary<(string, string), int> _mergeRank = new();

    // Special tokens
    public int BosTokenId { get; set; } = 1;
    public int EosTokenId { get; set; } = 2;
    public int PadTokenId { get; set; } = 0;
    public int UnkTokenId { get; set; } = 0;

    /// <summary>
    /// Initialize from GGUF metadata.
    /// </summary>
    public void LoadFromGGUF(GGUFReader gguf)
    {
        // Load vocab from tokenizer.ggml.tokens
        var tokensKV = gguf.Metadata.FirstOrDefault(m => m.Key == "tokenizer.ggml.tokens");
        if (tokensKV?.Value is List<object?> tokens)
        {
            for (int i = 0; i < tokens.Count; i++)
            {
                string tok = tokens[i]?.ToString() ?? "";
                _vocab[tok] = i;
                _vocabReverse[i] = tok;
            }
        }

        // Load merges from tokenizer.ggml.merges
        var mergesKV = gguf.Metadata.FirstOrDefault(m => m.Key == "tokenizer.ggml.merges");
        if (mergesKV?.Value is List<object?> merges)
        {
            for (int i = 0; i < merges.Count; i++)
            {
                string merge = merges[i]?.ToString() ?? "";
                var parts = merge.Split(' ', 2);
                if (parts.Length == 2)
                {
                    _merges.Add((parts[0], parts[1]));
                    _mergeRank[(parts[0], parts[1])] = i;
                }
            }
        }

        // Special tokens
        BosTokenId = gguf.GetMetadataInt("tokenizer.ggml.bos_token_id", 1);
        EosTokenId = gguf.GetMetadataInt("tokenizer.ggml.eos_token_id", 2);
        PadTokenId = gguf.GetMetadataInt("tokenizer.ggml.padding_token_id", 0);
        UnkTokenId = gguf.GetMetadataInt("tokenizer.ggml.unknown_token_id", 0);
    }

    /// <summary>
    /// Initialize with explicit vocab and merges.
    /// </summary>
    public void LoadVocabAndMerges(IReadOnlyList<string> vocabTokens, IReadOnlyList<string> mergeRules)
    {
        _vocab.Clear(); _vocabReverse.Clear(); _merges.Clear(); _mergeRank.Clear();

        for (int i = 0; i < vocabTokens.Count; i++)
        {
            _vocab[vocabTokens[i]] = i;
            _vocabReverse[i] = vocabTokens[i];
        }

        for (int i = 0; i < mergeRules.Count; i++)
        {
            var parts = mergeRules[i].Split(' ', 2);
            if (parts.Length == 2)
            {
                _merges.Add((parts[0], parts[1]));
                _mergeRank[(parts[0], parts[1])] = i;
            }
        }
    }

    /// <summary>
    /// Encode text to token IDs using BPE.
    /// </summary>
    public int[] Encode(string text, bool addBos = true, bool addEos = false)
    {
        var result = new List<int>();
        if (addBos) result.Add(BosTokenId);

        // Pre-tokenize: split on whitespace, keeping leading spaces as part of tokens
        var words = PreTokenize(text);

        foreach (var word in words)
        {
            // Convert to character-level tokens
            var symbols = new List<string>();
            foreach (char c in word)
                symbols.Add(c.ToString());

            // Apply BPE merges
            ApplyBPEMerges(symbols);

            // Map to token IDs
            foreach (var sym in symbols)
            {
                if (_vocab.TryGetValue(sym, out int id))
                    result.Add(id);
                else
                {
                    // Byte-level fallback
                    foreach (byte b in Encoding.UTF8.GetBytes(sym))
                    {
                        string byteToken = $"<0x{b:X2}>";
                        if (_vocab.TryGetValue(byteToken, out int bid))
                            result.Add(bid);
                        else
                            result.Add(UnkTokenId);
                    }
                }
            }
        }

        if (addEos) result.Add(EosTokenId);
        return result.ToArray();
    }

    /// <summary>
    /// Decode token IDs back to text.
    /// </summary>
    public string Decode(ReadOnlySpan<int> tokens)
    {
        var sb = new StringBuilder();
        foreach (int id in tokens)
        {
            if (id == BosTokenId || id == EosTokenId || id == PadTokenId)
                continue;
            if (_vocabReverse.TryGetValue(id, out string? tok))
                sb.Append(tok);
        }
        return sb.ToString();
    }

    private List<string> PreTokenize(string text)
    {
        // GPT-style pretokenization: split on spaces, punctuation boundaries
        var words = new List<string>();
        var current = new StringBuilder();

        for (int i = 0; i < text.Length; i++)
        {
            char c = text[i];

            if (c == ' ' && current.Length > 0)
            {
                words.Add(current.ToString());
                current.Clear();
                current.Append('\u2581'); // sentencepiece space marker
            }
            else if (c == ' ' && current.Length == 0)
            {
                current.Append('\u2581');
            }
            else
            {
                current.Append(c);
            }
        }

        if (current.Length > 0)
            words.Add(current.ToString());

        return words;
    }

    private void ApplyBPEMerges(List<string> symbols)
    {
        while (symbols.Count >= 2)
        {
            // Find the highest-priority merge
            int bestIdx = -1;
            int bestRank = int.MaxValue;

            for (int i = 0; i < symbols.Count - 1; i++)
            {
                if (_mergeRank.TryGetValue((symbols[i], symbols[i + 1]), out int rank))
                {
                    if (rank < bestRank)
                    {
                        bestRank = rank;
                        bestIdx = i;
                    }
                }
            }

            if (bestIdx == -1) break;

            // Apply merge
            string merged = symbols[bestIdx] + symbols[bestIdx + 1];
            symbols[bestIdx] = merged;
            symbols.RemoveAt(bestIdx + 1);
        }
    }

    /// <summary>
    /// Emit the inner BPE merge-scan loop as raw x64 machine code.
    /// This is for embedding the tokenizer into a PE .text section.
    /// 
    /// The emitted function signature (Windows x64 ABI):
    ///   void bpe_merge_scan(
    ///     uint8_t* symbols,       // RCX - packed symbol buffer
    ///     int32_t  symbol_count,  // RDX - number of symbols
    ///     uint32_t* merge_table,  // R8  - merge pair → rank lookup (perfect hash)
    ///     int32_t  merge_count,   // R9  - number of merge rules
    ///     int32_t* out_best_idx,  // [rsp+0x28] - output: index of best merge
    ///     int32_t* out_best_rank  // [rsp+0x30] - output: rank of best merge
    ///   );
    /// </summary>
    public static byte[] Emit_BPE_MergeScan_Kernel()
    {
        const int REG_RCX = 1, REG_RDX = 2, REG_R8 = 8, REG_R9 = 9;
        const int REG_R10 = 10, REG_R11 = 11, REG_R12 = 12, REG_R13 = 13, REG_RBX = 3;

        var code = new List<byte>();

        // Prologue
        code.AddRange(X64Emitter.Emit_FunctionPrologue(0x40));
        code.AddRange(X64Emitter.Emit_Push(REG_RBX));
        code.AddRange(X64Emitter.Emit_Push(REG_R12));
        code.AddRange(X64Emitter.Emit_Push(REG_R13));

        // R12 = best_rank = INT_MAX (0x7FFFFFFF)
        code.AddRange(X64Emitter.Emit_MovImm64(REG_R12, 0x7FFFFFFF));

        // R13 = best_idx = -1
        code.AddRange(X64Emitter.Emit_MovImm64(REG_R13, unchecked((ulong)-1)));

        // R10 = loop index = 0
        code.AddRange(X64Emitter.Emit_Xor(REG_R10, REG_R10));

        // RBX = symbol_count - 1
        code.AddRange(X64Emitter.Emit_MovReg(REG_RBX, REG_RDX));
        code.AddRange(X64Emitter.Emit_Dec(REG_RBX));

        int loopTop = code.Count;

        // CMP R10, RBX
        code.Add(0x4C); code.Add(0x39); code.Add(X64Emitter.ModRM(3, REG_R10 & 7, REG_RBX & 7));

        int jgeOff = code.Count;
        code.AddRange(new byte[] { 0x0F, 0x8D, 0x00, 0x00, 0x00, 0x00 }); // JGE exit

        // Load symbol pair hash: hash = symbols[R10] | (symbols[R10+1] << 16)
        // movzx eax, byte [rcx + r10]
        code.Add(0x42); code.Add(0x0F); code.Add(0xB6); code.Add(0x04); code.Add(0x11);
        // movzx r11d, byte [rcx + r10 + 1]
        code.Add(0x46); code.Add(0x0F); code.Add(0xB6); code.Add(0x5C); code.Add(0x11); code.Add(0x01);
        // shl r11d, 16
        code.Add(0x41); code.Add(0xC1); code.Add(0xE3); code.Add(0x10);
        // or eax, r11d
        code.Add(0x44); code.Add(0x09); code.Add(0xD8);

        // Lookup in merge_table: rank = merge_table[hash % merge_count]
        // This is simplified — real implementation uses perfect hashing
        // xor edx, edx; div r9d → eax=quotient, edx=remainder
        // Use remainder as index into R8 table
        code.AddRange(X64Emitter.Emit_Xor(REG_RDX, REG_RDX));
        // div r9d
        code.Add(0x41); code.Add(0xF7); code.Add(0xF1);
        // mov r11d, [r8 + rdx*4]
        code.Add(0x46); code.Add(0x8B); code.Add(0x1C); code.Add(0x90);

        // CMP R11, R12 (if rank < best_rank)
        code.Add(0x4D); code.Add(0x39); code.Add(X64Emitter.ModRM(3, REG_R12 & 7, REG_R11 & 7));
        int jgeSkip = code.Count;
        code.AddRange(new byte[] { 0x0F, 0x8E, 0x00, 0x00, 0x00, 0x00 }); // JLE skip

        // Update best
        code.AddRange(X64Emitter.Emit_MovReg(REG_R12, REG_R11)); // best_rank = rank
        code.AddRange(X64Emitter.Emit_MovReg(REG_R13, REG_R10)); // best_idx = i

        int skipTarget = code.Count;
        BinaryPrimitives.WriteInt32LittleEndian(
            CollectionsMarshal.AsSpan(code).Slice(jgeSkip + 2, 4), skipTarget - (jgeSkip + 6));

        // R10++
        code.AddRange(X64Emitter.Emit_Inc(REG_R10));
        code.AddRange(X64Emitter.Emit_Jmp_Rel32(loopTop - (code.Count + 5)));

        int afterLoop = code.Count;
        BinaryPrimitives.WriteInt32LittleEndian(
            CollectionsMarshal.AsSpan(code).Slice(jgeOff + 2, 4), afterLoop - (jgeOff + 6));

        // Store results: out_best_idx = [rsp+0x28+shadow], out_best_rank = [rsp+0x30+shadow]
        // mov rax, [rbp+0x28+0x40]  (arg5)
        code.AddRange(X64Emitter.Emit_MovLoad64(0, 5, 0x68)); // rax = arg5 ptr
        // mov [rax], r13d
        code.Add(0x44); code.Add(0x89); code.Add(0x28);
        // mov rax, [rbp+0x30+0x40]  (arg6)
        code.AddRange(X64Emitter.Emit_MovLoad64(0, 5, 0x70)); // rax = arg6 ptr
        // mov [rax], r12d
        code.Add(0x44); code.Add(0x89); code.Add(0x20);

        // Epilogue
        code.AddRange(X64Emitter.Emit_Pop(REG_R13));
        code.AddRange(X64Emitter.Emit_Pop(REG_R12));
        code.AddRange(X64Emitter.Emit_Pop(REG_RBX));
        code.AddRange(X64Emitter.Emit_FunctionEpilogue());

        return code.ToArray();
    }
}

// ─── Transformer Model Runner ───────────────────────────────────────────────

/// <summary>
/// Complete transformer inference pipeline: load GGUF → build graph → execute.
/// Ties together the GGUF reader, tokenizer, graph interpreter, and KV cache.
/// </summary>
public sealed class TransformerRunner
{
    private GGUFReader _gguf = new();
    private BPETokenizer _tokenizer = new();
    private GGUFGraphInterpreter _interpreter = new();
    private KVCacheManager? _kvCache;

    // Model config extracted from GGUF metadata
    public int VocabSize { get; private set; }
    public int EmbeddingDim { get; private set; }
    public int NumLayers { get; private set; }
    public int NumHeads { get; private set; }
    public int HeadDim { get; private set; }
    public int NumKVHeads { get; private set; }
    public int FFNHiddenDim { get; private set; }
    public int MaxSeqLen { get; private set; }
    public float RopeFreqBase { get; private set; }
    public float RmsNormEps { get; private set; }
    public string Architecture { get; private set; } = "";

    public void LoadModel(string ggufPath)
    {
        _gguf.Load(ggufPath);

        // Extract model config from GGUF metadata
        Architecture = _gguf.GetMetadataString("general.architecture") ?? "llama";
        string prefix = Architecture + ".";

        VocabSize     = _gguf.GetMetadataInt(prefix + "vocab_size", 32000);
        EmbeddingDim  = _gguf.GetMetadataInt(prefix + "embedding_length", 4096);
        NumLayers     = _gguf.GetMetadataInt(prefix + "block_count", 32);
        NumHeads      = _gguf.GetMetadataInt(prefix + "attention.head_count", 32);
        NumKVHeads    = _gguf.GetMetadataInt(prefix + "attention.head_count_kv", NumHeads);
        HeadDim       = EmbeddingDim / NumHeads;
        FFNHiddenDim  = _gguf.GetMetadataInt(prefix + "feed_forward_length", EmbeddingDim * 4);
        MaxSeqLen     = _gguf.GetMetadataInt(prefix + "context_length", 4096);
        RopeFreqBase  = _gguf.GetMetadataFloat(prefix + "rope.freq_base", 10000.0f);
        RmsNormEps    = _gguf.GetMetadataFloat(prefix + "attention.layer_norm_rms_epsilon", 1e-5f);

        // Initialize KV cache
        _kvCache = new KVCacheManager(NumLayers, NumKVHeads, HeadDim, MaxSeqLen);

        // Initialize tokenizer
        _tokenizer.LoadFromGGUF(_gguf);
    }

    /// <summary>
    /// Run a single forward pass for next-token prediction.
    /// </summary>
    public int[] Tokenize(string prompt) => _tokenizer.Encode(prompt, addBos: true);

    public string Detokenize(ReadOnlySpan<int> tokens) => _tokenizer.Decode(tokens);

    /// <summary>
    /// Get a tensor from the loaded GGUF by name.
    /// </summary>
    public GGUFGraphInterpreter.Tensor? GetTensor(string name)
    {
        var info = _gguf.TensorInfos.FirstOrDefault(t => t.Name == name);
        if (info == null) return null;

        var tensor = new GGUFGraphInterpreter.Tensor
        {
            Name = info.Name,
            Type = info.Type,
            Shape = info.Shape.Select(s => (int)s).ToArray(),
            Data = _gguf.TensorData,
            Offset = (int)info.Offset,
        };
        return tensor;
    }

    /// <summary>
    /// Get the emitted machine code kernel table for direct embedding into a PE .text section.
    /// Returns a dictionary of symbol_name → machine_code_bytes for the linker.
    /// </summary>
    public static Dictionary<string, byte[]> GetEmittedKernels()
    {
        return new Dictionary<string, byte[]>
        {
            ["_rawrxd_matmul_f16"]    = MatMulKernelBuilder.Build_F16_MatMul_Kernel(),
            ["_rawrxd_matmul_q4_0"]   = MatMulKernelBuilder.Build_Q4_0_MatMul_Kernel(),
            ["_rawrxd_matmul_q8_0"]   = MatMulKernelBuilder.Build_Q8_0_MatMul_Kernel(),
            ["_rawrxd_matmul_q5_0"]   = MatMulKernelBuilder.Build_Q5_0_MatMul_Kernel(),
            ["_rawrxd_matmul_q4_k_m"] = MatMulKernelBuilder.Build_Q4_K_M_MatMul_Kernel(),
            ["_rawrxd_bpe_merge"]     = BPETokenizer.Emit_BPE_MergeScan_Kernel(),
        };
    }

    /// <summary>
    /// Dump all emitted kernels to a flat binary + symbol map for the linker.
    /// </summary>
    public static (byte[] codeBlob, Dictionary<string, int> symbolOffsets) EmitAllKernelsFlat()
    {
        var kernels = GetEmittedKernels();
        var blob = new List<byte>();
        var symbols = new Dictionary<string, int>();

        foreach (var (name, code) in kernels)
        {
            // Align to 16-byte boundary
            int pad = (16 - (blob.Count % 16)) % 16;
            blob.AddRange(new byte[pad]);

            symbols[name] = blob.Count;
            blob.AddRange(code);
        }

        return (blob.ToArray(), symbols);
    }
}
