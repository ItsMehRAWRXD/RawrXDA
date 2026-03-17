#pragma once

// ============================================================================
// RAWRXD WEBASSEMBLY CODE GENERATOR
// Complete WASM target with full instruction set support
// ============================================================================

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <variant>
#include <optional>
#include <functional>
#include <sstream>

namespace RawrXD {
namespace Compiler {
namespace WASM {

// ============================================================================
// WASM BINARY FORMAT CONSTANTS
// ============================================================================

namespace WASMSection {
    constexpr uint8_t Custom = 0;
    constexpr uint8_t Type = 1;
    constexpr uint8_t Import = 2;
    constexpr uint8_t Function = 3;
    constexpr uint8_t Table = 4;
    constexpr uint8_t Memory = 5;
    constexpr uint8_t Global = 6;
    constexpr uint8_t Export = 7;
    constexpr uint8_t Start = 8;
    constexpr uint8_t Element = 9;
    constexpr uint8_t Code = 10;
    constexpr uint8_t Data = 11;
    constexpr uint8_t DataCount = 12;
}

namespace WASMOpcode {
    // Control flow
    constexpr uint8_t Unreachable = 0x00;
    constexpr uint8_t Nop = 0x01;
    constexpr uint8_t Block = 0x02;
    constexpr uint8_t Loop = 0x03;
    constexpr uint8_t If = 0x04;
    constexpr uint8_t Else = 0x05;
    constexpr uint8_t End = 0x0B;
    constexpr uint8_t Br = 0x0C;
    constexpr uint8_t BrIf = 0x0D;
    constexpr uint8_t BrTable = 0x0E;
    constexpr uint8_t Return = 0x0F;
    constexpr uint8_t Call = 0x10;
    constexpr uint8_t CallIndirect = 0x11;
    
    // Parametric
    constexpr uint8_t Drop = 0x1A;
    constexpr uint8_t Select = 0x1B;
    
    // Variable
    constexpr uint8_t LocalGet = 0x20;
    constexpr uint8_t LocalSet = 0x21;
    constexpr uint8_t LocalTee = 0x22;
    constexpr uint8_t GlobalGet = 0x23;
    constexpr uint8_t GlobalSet = 0x24;
    
    // Memory
    constexpr uint8_t I32Load = 0x28;
    constexpr uint8_t I64Load = 0x29;
    constexpr uint8_t F32Load = 0x2A;
    constexpr uint8_t F64Load = 0x2B;
    constexpr uint8_t I32Load8S = 0x2C;
    constexpr uint8_t I32Load8U = 0x2D;
    constexpr uint8_t I32Load16S = 0x2E;
    constexpr uint8_t I32Load16U = 0x2F;
    constexpr uint8_t I64Load8S = 0x30;
    constexpr uint8_t I64Load8U = 0x31;
    constexpr uint8_t I64Load16S = 0x32;
    constexpr uint8_t I64Load16U = 0x33;
    constexpr uint8_t I64Load32S = 0x34;
    constexpr uint8_t I64Load32U = 0x35;
    constexpr uint8_t I32Store = 0x36;
    constexpr uint8_t I64Store = 0x37;
    constexpr uint8_t F32Store = 0x38;
    constexpr uint8_t F64Store = 0x39;
    constexpr uint8_t I32Store8 = 0x3A;
    constexpr uint8_t I32Store16 = 0x3B;
    constexpr uint8_t I64Store8 = 0x3C;
    constexpr uint8_t I64Store16 = 0x3D;
    constexpr uint8_t I64Store32 = 0x3E;
    constexpr uint8_t MemorySize = 0x3F;
    constexpr uint8_t MemoryGrow = 0x40;
    
    // Numeric - i32
    constexpr uint8_t I32Const = 0x41;
    constexpr uint8_t I64Const = 0x42;
    constexpr uint8_t F32Const = 0x43;
    constexpr uint8_t F64Const = 0x44;
    
    constexpr uint8_t I32Eqz = 0x45;
    constexpr uint8_t I32Eq = 0x46;
    constexpr uint8_t I32Ne = 0x47;
    constexpr uint8_t I32LtS = 0x48;
    constexpr uint8_t I32LtU = 0x49;
    constexpr uint8_t I32GtS = 0x4A;
    constexpr uint8_t I32GtU = 0x4B;
    constexpr uint8_t I32LeS = 0x4C;
    constexpr uint8_t I32LeU = 0x4D;
    constexpr uint8_t I32GeS = 0x4E;
    constexpr uint8_t I32GeU = 0x4F;
    
    constexpr uint8_t I64Eqz = 0x50;
    constexpr uint8_t I64Eq = 0x51;
    constexpr uint8_t I64Ne = 0x52;
    constexpr uint8_t I64LtS = 0x53;
    constexpr uint8_t I64LtU = 0x54;
    constexpr uint8_t I64GtS = 0x55;
    constexpr uint8_t I64GtU = 0x56;
    constexpr uint8_t I64LeS = 0x57;
    constexpr uint8_t I64LeU = 0x58;
    constexpr uint8_t I64GeS = 0x59;
    constexpr uint8_t I64GeU = 0x5A;
    
    constexpr uint8_t F32Eq = 0x5B;
    constexpr uint8_t F32Ne = 0x5C;
    constexpr uint8_t F32Lt = 0x5D;
    constexpr uint8_t F32Gt = 0x5E;
    constexpr uint8_t F32Le = 0x5F;
    constexpr uint8_t F32Ge = 0x60;
    
    constexpr uint8_t F64Eq = 0x61;
    constexpr uint8_t F64Ne = 0x62;
    constexpr uint8_t F64Lt = 0x63;
    constexpr uint8_t F64Gt = 0x64;
    constexpr uint8_t F64Le = 0x65;
    constexpr uint8_t F64Ge = 0x66;
    
    constexpr uint8_t I32Clz = 0x67;
    constexpr uint8_t I32Ctz = 0x68;
    constexpr uint8_t I32Popcnt = 0x69;
    constexpr uint8_t I32Add = 0x6A;
    constexpr uint8_t I32Sub = 0x6B;
    constexpr uint8_t I32Mul = 0x6C;
    constexpr uint8_t I32DivS = 0x6D;
    constexpr uint8_t I32DivU = 0x6E;
    constexpr uint8_t I32RemS = 0x6F;
    constexpr uint8_t I32RemU = 0x70;
    constexpr uint8_t I32And = 0x71;
    constexpr uint8_t I32Or = 0x72;
    constexpr uint8_t I32Xor = 0x73;
    constexpr uint8_t I32Shl = 0x74;
    constexpr uint8_t I32ShrS = 0x75;
    constexpr uint8_t I32ShrU = 0x76;
    constexpr uint8_t I32Rotl = 0x77;
    constexpr uint8_t I32Rotr = 0x78;
    
    constexpr uint8_t I64Clz = 0x79;
    constexpr uint8_t I64Ctz = 0x7A;
    constexpr uint8_t I64Popcnt = 0x7B;
    constexpr uint8_t I64Add = 0x7C;
    constexpr uint8_t I64Sub = 0x7D;
    constexpr uint8_t I64Mul = 0x7E;
    constexpr uint8_t I64DivS = 0x7F;
    constexpr uint8_t I64DivU = 0x80;
    constexpr uint8_t I64RemS = 0x81;
    constexpr uint8_t I64RemU = 0x82;
    constexpr uint8_t I64And = 0x83;
    constexpr uint8_t I64Or = 0x84;
    constexpr uint8_t I64Xor = 0x85;
    constexpr uint8_t I64Shl = 0x86;
    constexpr uint8_t I64ShrS = 0x87;
    constexpr uint8_t I64ShrU = 0x88;
    constexpr uint8_t I64Rotl = 0x89;
    constexpr uint8_t I64Rotr = 0x8A;
    
    constexpr uint8_t F32Abs = 0x8B;
    constexpr uint8_t F32Neg = 0x8C;
    constexpr uint8_t F32Ceil = 0x8D;
    constexpr uint8_t F32Floor = 0x8E;
    constexpr uint8_t F32Trunc = 0x8F;
    constexpr uint8_t F32Nearest = 0x90;
    constexpr uint8_t F32Sqrt = 0x91;
    constexpr uint8_t F32Add = 0x92;
    constexpr uint8_t F32Sub = 0x93;
    constexpr uint8_t F32Mul = 0x94;
    constexpr uint8_t F32Div = 0x95;
    constexpr uint8_t F32Min = 0x96;
    constexpr uint8_t F32Max = 0x97;
    constexpr uint8_t F32Copysign = 0x98;
    
    constexpr uint8_t F64Abs = 0x99;
    constexpr uint8_t F64Neg = 0x9A;
    constexpr uint8_t F64Ceil = 0x9B;
    constexpr uint8_t F64Floor = 0x9C;
    constexpr uint8_t F64Trunc = 0x9D;
    constexpr uint8_t F64Nearest = 0x9E;
    constexpr uint8_t F64Sqrt = 0x9F;
    constexpr uint8_t F64Add = 0xA0;
    constexpr uint8_t F64Sub = 0xA1;
    constexpr uint8_t F64Mul = 0xA2;
    constexpr uint8_t F64Div = 0xA3;
    constexpr uint8_t F64Min = 0xA4;
    constexpr uint8_t F64Max = 0xA5;
    constexpr uint8_t F64Copysign = 0xA6;
    
    // Conversions
    constexpr uint8_t I32WrapI64 = 0xA7;
    constexpr uint8_t I32TruncF32S = 0xA8;
    constexpr uint8_t I32TruncF32U = 0xA9;
    constexpr uint8_t I32TruncF64S = 0xAA;
    constexpr uint8_t I32TruncF64U = 0xAB;
    constexpr uint8_t I64ExtendI32S = 0xAC;
    constexpr uint8_t I64ExtendI32U = 0xAD;
    constexpr uint8_t I64TruncF32S = 0xAE;
    constexpr uint8_t I64TruncF32U = 0xAF;
    constexpr uint8_t I64TruncF64S = 0xB0;
    constexpr uint8_t I64TruncF64U = 0xB1;
    constexpr uint8_t F32ConvertI32S = 0xB2;
    constexpr uint8_t F32ConvertI32U = 0xB3;
    constexpr uint8_t F32ConvertI64S = 0xB4;
    constexpr uint8_t F32ConvertI64U = 0xB5;
    constexpr uint8_t F32DemoteF64 = 0xB6;
    constexpr uint8_t F64ConvertI32S = 0xB7;
    constexpr uint8_t F64ConvertI32U = 0xB8;
    constexpr uint8_t F64ConvertI64S = 0xB9;
    constexpr uint8_t F64ConvertI64U = 0xBA;
    constexpr uint8_t F64PromoteF32 = 0xBB;
    constexpr uint8_t I32ReinterpretF32 = 0xBC;
    constexpr uint8_t I64ReinterpretF64 = 0xBD;
    constexpr uint8_t F32ReinterpretI32 = 0xBE;
    constexpr uint8_t F64ReinterpretI64 = 0xBF;
    
    // Sign extension
    constexpr uint8_t I32Extend8S = 0xC0;
    constexpr uint8_t I32Extend16S = 0xC1;
    constexpr uint8_t I64Extend8S = 0xC2;
    constexpr uint8_t I64Extend16S = 0xC3;
    constexpr uint8_t I64Extend32S = 0xC4;
}

// ============================================================================
// WASM VALUE TYPES
// ============================================================================

enum class ValueType : uint8_t {
    I32 = 0x7F,
    I64 = 0x7E,
    F32 = 0x7D,
    F64 = 0x7C,
    V128 = 0x7B,    // SIMD
    FuncRef = 0x70,
    ExternRef = 0x6F,
    Void = 0x40
};

struct FunctionType {
    std::vector<ValueType> params;
    std::vector<ValueType> results;
    
    bool operator==(const FunctionType& other) const {
        return params == other.params && results == other.results;
    }
};

// ============================================================================
// WASM MODULE BUILDER
// ============================================================================

/**
 * @brief Binary writer for WASM format
 */
class WASMBinaryWriter {
public:
    void writeByte(uint8_t b) {
        buffer_.push_back(b);
    }
    
    void writeBytes(const std::vector<uint8_t>& bytes) {
        buffer_.insert(buffer_.end(), bytes.begin(), bytes.end());
    }
    
    void writeBytes(const uint8_t* data, size_t size) {
        buffer_.insert(buffer_.end(), data, data + size);
    }
    
    // LEB128 encoding
    void writeU32LEB(uint32_t value) {
        do {
            uint8_t byte = value & 0x7F;
            value >>= 7;
            if (value != 0) byte |= 0x80;
            writeByte(byte);
        } while (value != 0);
    }
    
    void writeS32LEB(int32_t value) {
        bool more = true;
        while (more) {
            uint8_t byte = value & 0x7F;
            value >>= 7;
            if ((value == 0 && (byte & 0x40) == 0) ||
                (value == -1 && (byte & 0x40) != 0)) {
                more = false;
            } else {
                byte |= 0x80;
            }
            writeByte(byte);
        }
    }
    
    void writeS64LEB(int64_t value) {
        bool more = true;
        while (more) {
            uint8_t byte = value & 0x7F;
            value >>= 7;
            if ((value == 0 && (byte & 0x40) == 0) ||
                (value == -1 && (byte & 0x40) != 0)) {
                more = false;
            } else {
                byte |= 0x80;
            }
            writeByte(byte);
        }
    }
    
    void writeU64LEB(uint64_t value) {
        do {
            uint8_t byte = value & 0x7F;
            value >>= 7;
            if (value != 0) byte |= 0x80;
            writeByte(byte);
        } while (value != 0);
    }
    
    void writeF32(float value) {
        uint32_t bits;
        memcpy(&bits, &value, sizeof(bits));
        writeByte(bits & 0xFF);
        writeByte((bits >> 8) & 0xFF);
        writeByte((bits >> 16) & 0xFF);
        writeByte((bits >> 24) & 0xFF);
    }
    
    void writeF64(double value) {
        uint64_t bits;
        memcpy(&bits, &value, sizeof(bits));
        for (int i = 0; i < 8; ++i) {
            writeByte((bits >> (i * 8)) & 0xFF);
        }
    }
    
    void writeString(const std::string& str) {
        writeU32LEB(static_cast<uint32_t>(str.size()));
        writeBytes(reinterpret_cast<const uint8_t*>(str.data()), str.size());
    }
    
    void writeValueType(ValueType type) {
        writeByte(static_cast<uint8_t>(type));
    }
    
    void writeFunctionType(const FunctionType& type) {
        writeByte(0x60); // func type marker
        writeU32LEB(static_cast<uint32_t>(type.params.size()));
        for (auto t : type.params) writeValueType(t);
        writeU32LEB(static_cast<uint32_t>(type.results.size()));
        for (auto t : type.results) writeValueType(t);
    }
    
    const std::vector<uint8_t>& getBuffer() const { return buffer_; }
    std::vector<uint8_t>& getBuffer() { return buffer_; }
    size_t size() const { return buffer_.size(); }
    void clear() { buffer_.clear(); }
    
    // Create a sub-writer for section content
    WASMBinaryWriter createSubWriter() const { return WASMBinaryWriter(); }
    
private:
    std::vector<uint8_t> buffer_;
};

/**
 * @brief Instruction builder for WASM functions
 */
class WASMInstructionBuilder {
public:
    WASMInstructionBuilder& unreachable() { emit(WASMOpcode::Unreachable); return *this; }
    WASMInstructionBuilder& nop() { emit(WASMOpcode::Nop); return *this; }
    
    WASMInstructionBuilder& block(ValueType resultType = ValueType::Void) {
        emit(WASMOpcode::Block);
        emit(static_cast<uint8_t>(resultType));
        return *this;
    }
    
    WASMInstructionBuilder& loop(ValueType resultType = ValueType::Void) {
        emit(WASMOpcode::Loop);
        emit(static_cast<uint8_t>(resultType));
        return *this;
    }
    
    WASMInstructionBuilder& if_(ValueType resultType = ValueType::Void) {
        emit(WASMOpcode::If);
        emit(static_cast<uint8_t>(resultType));
        return *this;
    }
    
    WASMInstructionBuilder& else_() { emit(WASMOpcode::Else); return *this; }
    WASMInstructionBuilder& end() { emit(WASMOpcode::End); return *this; }
    
    WASMInstructionBuilder& br(uint32_t depth) {
        emit(WASMOpcode::Br);
        emitU32LEB(depth);
        return *this;
    }
    
    WASMInstructionBuilder& brIf(uint32_t depth) {
        emit(WASMOpcode::BrIf);
        emitU32LEB(depth);
        return *this;
    }
    
    WASMInstructionBuilder& brTable(const std::vector<uint32_t>& labels, uint32_t defaultLabel) {
        emit(WASMOpcode::BrTable);
        emitU32LEB(static_cast<uint32_t>(labels.size()));
        for (auto l : labels) emitU32LEB(l);
        emitU32LEB(defaultLabel);
        return *this;
    }
    
    WASMInstructionBuilder& return_() { emit(WASMOpcode::Return); return *this; }
    
    WASMInstructionBuilder& call(uint32_t funcIndex) {
        emit(WASMOpcode::Call);
        emitU32LEB(funcIndex);
        return *this;
    }
    
    WASMInstructionBuilder& callIndirect(uint32_t typeIndex, uint32_t tableIndex = 0) {
        emit(WASMOpcode::CallIndirect);
        emitU32LEB(typeIndex);
        emitU32LEB(tableIndex);
        return *this;
    }
    
    WASMInstructionBuilder& drop() { emit(WASMOpcode::Drop); return *this; }
    WASMInstructionBuilder& select() { emit(WASMOpcode::Select); return *this; }
    
    WASMInstructionBuilder& localGet(uint32_t index) {
        emit(WASMOpcode::LocalGet);
        emitU32LEB(index);
        return *this;
    }
    
    WASMInstructionBuilder& localSet(uint32_t index) {
        emit(WASMOpcode::LocalSet);
        emitU32LEB(index);
        return *this;
    }
    
    WASMInstructionBuilder& localTee(uint32_t index) {
        emit(WASMOpcode::LocalTee);
        emitU32LEB(index);
        return *this;
    }
    
    WASMInstructionBuilder& globalGet(uint32_t index) {
        emit(WASMOpcode::GlobalGet);
        emitU32LEB(index);
        return *this;
    }
    
    WASMInstructionBuilder& globalSet(uint32_t index) {
        emit(WASMOpcode::GlobalSet);
        emitU32LEB(index);
        return *this;
    }
    
    // Memory operations
    WASMInstructionBuilder& i32Load(uint32_t align = 2, uint32_t offset = 0) {
        emit(WASMOpcode::I32Load);
        emitU32LEB(align);
        emitU32LEB(offset);
        return *this;
    }
    
    WASMInstructionBuilder& i64Load(uint32_t align = 3, uint32_t offset = 0) {
        emit(WASMOpcode::I64Load);
        emitU32LEB(align);
        emitU32LEB(offset);
        return *this;
    }
    
    WASMInstructionBuilder& f32Load(uint32_t align = 2, uint32_t offset = 0) {
        emit(WASMOpcode::F32Load);
        emitU32LEB(align);
        emitU32LEB(offset);
        return *this;
    }
    
    WASMInstructionBuilder& f64Load(uint32_t align = 3, uint32_t offset = 0) {
        emit(WASMOpcode::F64Load);
        emitU32LEB(align);
        emitU32LEB(offset);
        return *this;
    }
    
    WASMInstructionBuilder& i32Store(uint32_t align = 2, uint32_t offset = 0) {
        emit(WASMOpcode::I32Store);
        emitU32LEB(align);
        emitU32LEB(offset);
        return *this;
    }
    
    WASMInstructionBuilder& i64Store(uint32_t align = 3, uint32_t offset = 0) {
        emit(WASMOpcode::I64Store);
        emitU32LEB(align);
        emitU32LEB(offset);
        return *this;
    }
    
    WASMInstructionBuilder& memorySize() { emit(WASMOpcode::MemorySize); emit(0); return *this; }
    WASMInstructionBuilder& memoryGrow() { emit(WASMOpcode::MemoryGrow); emit(0); return *this; }
    
    // Constants
    WASMInstructionBuilder& i32Const(int32_t value) {
        emit(WASMOpcode::I32Const);
        emitS32LEB(value);
        return *this;
    }
    
    WASMInstructionBuilder& i64Const(int64_t value) {
        emit(WASMOpcode::I64Const);
        emitS64LEB(value);
        return *this;
    }
    
    WASMInstructionBuilder& f32Const(float value) {
        emit(WASMOpcode::F32Const);
        emitF32(value);
        return *this;
    }
    
    WASMInstructionBuilder& f64Const(double value) {
        emit(WASMOpcode::F64Const);
        emitF64(value);
        return *this;
    }
    
    // i32 comparison
    WASMInstructionBuilder& i32Eqz() { emit(WASMOpcode::I32Eqz); return *this; }
    WASMInstructionBuilder& i32Eq() { emit(WASMOpcode::I32Eq); return *this; }
    WASMInstructionBuilder& i32Ne() { emit(WASMOpcode::I32Ne); return *this; }
    WASMInstructionBuilder& i32LtS() { emit(WASMOpcode::I32LtS); return *this; }
    WASMInstructionBuilder& i32LtU() { emit(WASMOpcode::I32LtU); return *this; }
    WASMInstructionBuilder& i32GtS() { emit(WASMOpcode::I32GtS); return *this; }
    WASMInstructionBuilder& i32GtU() { emit(WASMOpcode::I32GtU); return *this; }
    WASMInstructionBuilder& i32LeS() { emit(WASMOpcode::I32LeS); return *this; }
    WASMInstructionBuilder& i32LeU() { emit(WASMOpcode::I32LeU); return *this; }
    WASMInstructionBuilder& i32GeS() { emit(WASMOpcode::I32GeS); return *this; }
    WASMInstructionBuilder& i32GeU() { emit(WASMOpcode::I32GeU); return *this; }
    
    // i32 arithmetic
    WASMInstructionBuilder& i32Add() { emit(WASMOpcode::I32Add); return *this; }
    WASMInstructionBuilder& i32Sub() { emit(WASMOpcode::I32Sub); return *this; }
    WASMInstructionBuilder& i32Mul() { emit(WASMOpcode::I32Mul); return *this; }
    WASMInstructionBuilder& i32DivS() { emit(WASMOpcode::I32DivS); return *this; }
    WASMInstructionBuilder& i32DivU() { emit(WASMOpcode::I32DivU); return *this; }
    WASMInstructionBuilder& i32RemS() { emit(WASMOpcode::I32RemS); return *this; }
    WASMInstructionBuilder& i32RemU() { emit(WASMOpcode::I32RemU); return *this; }
    WASMInstructionBuilder& i32And() { emit(WASMOpcode::I32And); return *this; }
    WASMInstructionBuilder& i32Or() { emit(WASMOpcode::I32Or); return *this; }
    WASMInstructionBuilder& i32Xor() { emit(WASMOpcode::I32Xor); return *this; }
    WASMInstructionBuilder& i32Shl() { emit(WASMOpcode::I32Shl); return *this; }
    WASMInstructionBuilder& i32ShrS() { emit(WASMOpcode::I32ShrS); return *this; }
    WASMInstructionBuilder& i32ShrU() { emit(WASMOpcode::I32ShrU); return *this; }
    
    // i64 operations
    WASMInstructionBuilder& i64Add() { emit(WASMOpcode::I64Add); return *this; }
    WASMInstructionBuilder& i64Sub() { emit(WASMOpcode::I64Sub); return *this; }
    WASMInstructionBuilder& i64Mul() { emit(WASMOpcode::I64Mul); return *this; }
    WASMInstructionBuilder& i64DivS() { emit(WASMOpcode::I64DivS); return *this; }
    WASMInstructionBuilder& i64DivU() { emit(WASMOpcode::I64DivU); return *this; }
    
    // f32 operations
    WASMInstructionBuilder& f32Add() { emit(WASMOpcode::F32Add); return *this; }
    WASMInstructionBuilder& f32Sub() { emit(WASMOpcode::F32Sub); return *this; }
    WASMInstructionBuilder& f32Mul() { emit(WASMOpcode::F32Mul); return *this; }
    WASMInstructionBuilder& f32Div() { emit(WASMOpcode::F32Div); return *this; }
    WASMInstructionBuilder& f32Sqrt() { emit(WASMOpcode::F32Sqrt); return *this; }
    
    // f64 operations
    WASMInstructionBuilder& f64Add() { emit(WASMOpcode::F64Add); return *this; }
    WASMInstructionBuilder& f64Sub() { emit(WASMOpcode::F64Sub); return *this; }
    WASMInstructionBuilder& f64Mul() { emit(WASMOpcode::F64Mul); return *this; }
    WASMInstructionBuilder& f64Div() { emit(WASMOpcode::F64Div); return *this; }
    WASMInstructionBuilder& f64Sqrt() { emit(WASMOpcode::F64Sqrt); return *this; }
    
    // Conversions
    WASMInstructionBuilder& i32WrapI64() { emit(WASMOpcode::I32WrapI64); return *this; }
    WASMInstructionBuilder& i64ExtendI32S() { emit(WASMOpcode::I64ExtendI32S); return *this; }
    WASMInstructionBuilder& i64ExtendI32U() { emit(WASMOpcode::I64ExtendI32U); return *this; }
    WASMInstructionBuilder& f32ConvertI32S() { emit(WASMOpcode::F32ConvertI32S); return *this; }
    WASMInstructionBuilder& f64ConvertI64S() { emit(WASMOpcode::F64ConvertI64S); return *this; }
    WASMInstructionBuilder& i32TruncF32S() { emit(WASMOpcode::I32TruncF32S); return *this; }
    WASMInstructionBuilder& i64TruncF64S() { emit(WASMOpcode::I64TruncF64S); return *this; }
    
    const std::vector<uint8_t>& getCode() const { return code_; }
    void clear() { code_.clear(); }
    
private:
    void emit(uint8_t byte) { code_.push_back(byte); }
    
    void emitU32LEB(uint32_t value) {
        do {
            uint8_t byte = value & 0x7F;
            value >>= 7;
            if (value != 0) byte |= 0x80;
            emit(byte);
        } while (value != 0);
    }
    
    void emitS32LEB(int32_t value) {
        bool more = true;
        while (more) {
            uint8_t byte = value & 0x7F;
            value >>= 7;
            if ((value == 0 && (byte & 0x40) == 0) ||
                (value == -1 && (byte & 0x40) != 0)) {
                more = false;
            } else {
                byte |= 0x80;
            }
            emit(byte);
        }
    }
    
    void emitS64LEB(int64_t value) {
        bool more = true;
        while (more) {
            uint8_t byte = value & 0x7F;
            value >>= 7;
            if ((value == 0 && (byte & 0x40) == 0) ||
                (value == -1 && (byte & 0x40) != 0)) {
                more = false;
            } else {
                byte |= 0x80;
            }
            emit(byte);
        }
    }
    
    void emitF32(float value) {
        uint32_t bits;
        memcpy(&bits, &value, sizeof(bits));
        emit(bits & 0xFF);
        emit((bits >> 8) & 0xFF);
        emit((bits >> 16) & 0xFF);
        emit((bits >> 24) & 0xFF);
    }
    
    void emitF64(double value) {
        uint64_t bits;
        memcpy(&bits, &value, sizeof(bits));
        for (int i = 0; i < 8; ++i) {
            emit((bits >> (i * 8)) & 0xFF);
        }
    }
    
    std::vector<uint8_t> code_;
};

/**
 * @brief Complete WASM module builder
 */
class WASMModuleBuilder {
public:
    struct FunctionDef {
        std::string name;
        uint32_t typeIndex;
        std::vector<ValueType> locals;
        std::vector<uint8_t> code;
        bool exported = false;
    };
    
    struct ImportDef {
        std::string module;
        std::string name;
        enum Kind { Function, Table, Memory, Global } kind;
        uint32_t typeIndex; // For functions
    };
    
    struct GlobalDef {
        ValueType type;
        bool mutable_;
        std::vector<uint8_t> initExpr;
        std::string exportName;
    };
    
    struct DataSegment {
        uint32_t memoryIndex = 0;
        std::vector<uint8_t> offsetExpr;
        std::vector<uint8_t> data;
    };
    
    WASMModuleBuilder() = default;
    
    // Add type
    uint32_t addType(const FunctionType& type) {
        // Check for existing type
        for (size_t i = 0; i < types_.size(); ++i) {
            if (types_[i] == type) return static_cast<uint32_t>(i);
        }
        types_.push_back(type);
        return static_cast<uint32_t>(types_.size() - 1);
    }
    
    // Add import
    uint32_t addImport(const ImportDef& import) {
        imports_.push_back(import);
        if (import.kind == ImportDef::Function) {
            return numImportedFunctions_++;
        }
        return 0;
    }
    
    // Add function
    uint32_t addFunction(const FunctionDef& func) {
        functions_.push_back(func);
        return numImportedFunctions_ + static_cast<uint32_t>(functions_.size() - 1);
    }
    
    // Add memory
    void setMemory(uint32_t initial, uint32_t maximum = 0, bool hasMax = false, bool exported = true, const std::string& name = "memory") {
        memoryInitial_ = initial;
        memoryMaximum_ = maximum;
        memoryHasMax_ = hasMax;
        memoryExported_ = exported;
        memoryExportName_ = name;
    }
    
    // Add global
    uint32_t addGlobal(const GlobalDef& global) {
        globals_.push_back(global);
        return static_cast<uint32_t>(globals_.size() - 1);
    }
    
    // Add data segment
    void addDataSegment(const DataSegment& segment) {
        dataSegments_.push_back(segment);
    }
    
    // Add string data
    uint32_t addStringData(const std::string& str, uint32_t offset) {
        DataSegment seg;
        seg.memoryIndex = 0;
        
        // Offset expression: i32.const offset
        seg.offsetExpr.push_back(WASMOpcode::I32Const);
        // LEB128 encode offset
        uint32_t val = offset;
        do {
            uint8_t byte = val & 0x7F;
            val >>= 7;
            if (val != 0) byte |= 0x80;
            seg.offsetExpr.push_back(byte);
        } while (val != 0);
        seg.offsetExpr.push_back(WASMOpcode::End);
        
        seg.data = std::vector<uint8_t>(str.begin(), str.end());
        seg.data.push_back(0); // Null terminator
        
        dataSegments_.push_back(seg);
        return offset;
    }
    
    // Set start function
    void setStartFunction(uint32_t funcIndex) {
        startFunction_ = funcIndex;
        hasStartFunction_ = true;
    }
    
    // Build the complete module
    std::vector<uint8_t> build() {
        WASMBinaryWriter writer;
        
        // Magic number and version
        writer.writeByte(0x00); writer.writeByte(0x61);
        writer.writeByte(0x73); writer.writeByte(0x6D); // \0asm
        writer.writeByte(0x01); writer.writeByte(0x00);
        writer.writeByte(0x00); writer.writeByte(0x00); // version 1
        
        // Type section
        if (!types_.empty()) {
            writeTypeSection(writer);
        }
        
        // Import section
        if (!imports_.empty()) {
            writeImportSection(writer);
        }
        
        // Function section
        if (!functions_.empty()) {
            writeFunctionSection(writer);
        }
        
        // Memory section
        writeMemorySection(writer);
        
        // Global section
        if (!globals_.empty()) {
            writeGlobalSection(writer);
        }
        
        // Export section
        writeExportSection(writer);
        
        // Start section
        if (hasStartFunction_) {
            writeStartSection(writer);
        }
        
        // Data count section (for bulk memory)
        if (!dataSegments_.empty()) {
            writeDataCountSection(writer);
        }
        
        // Code section
        if (!functions_.empty()) {
            writeCodeSection(writer);
        }
        
        // Data section
        if (!dataSegments_.empty()) {
            writeDataSection(writer);
        }
        
        return writer.getBuffer();
    }
    
    // Generate WAT (text format) for debugging
    std::string toWAT() const {
        std::ostringstream oss;
        oss << "(module\n";
        
        // Types
        for (size_t i = 0; i < types_.size(); ++i) {
            oss << "  (type (;$t" << i << ";) (func";
            if (!types_[i].params.empty()) {
                oss << " (param";
                for (auto t : types_[i].params) {
                    oss << " " << valueTypeToString(t);
                }
                oss << ")";
            }
            if (!types_[i].results.empty()) {
                oss << " (result";
                for (auto t : types_[i].results) {
                    oss << " " << valueTypeToString(t);
                }
                oss << ")";
            }
            oss << "))\n";
        }
        
        // Memory
        oss << "  (memory (;0;) " << memoryInitial_;
        if (memoryHasMax_) oss << " " << memoryMaximum_;
        oss << ")\n";
        
        // Exports
        if (memoryExported_) {
            oss << "  (export \"" << memoryExportName_ << "\" (memory 0))\n";
        }
        
        for (const auto& func : functions_) {
            if (func.exported) {
                oss << "  (export \"" << func.name << "\" (func $" << func.name << "))\n";
            }
        }
        
        // Functions
        for (const auto& func : functions_) {
            oss << "  (func $" << func.name << " (type " << func.typeIndex << ")";
            if (!func.locals.empty()) {
                oss << "\n    (local";
                for (auto t : func.locals) {
                    oss << " " << valueTypeToString(t);
                }
                oss << ")";
            }
            oss << "\n    ;; code...\n  )\n";
        }
        
        oss << ")\n";
        return oss.str();
    }
    
private:
    static std::string valueTypeToString(ValueType t) {
        switch (t) {
            case ValueType::I32: return "i32";
            case ValueType::I64: return "i64";
            case ValueType::F32: return "f32";
            case ValueType::F64: return "f64";
            default: return "unknown";
        }
    }
    
    void writeSection(WASMBinaryWriter& writer, uint8_t sectionId, const std::vector<uint8_t>& content) {
        writer.writeByte(sectionId);
        writer.writeU32LEB(static_cast<uint32_t>(content.size()));
        writer.writeBytes(content);
    }
    
    void writeTypeSection(WASMBinaryWriter& writer) {
        WASMBinaryWriter content;
        content.writeU32LEB(static_cast<uint32_t>(types_.size()));
        for (const auto& type : types_) {
            content.writeFunctionType(type);
        }
        writeSection(writer, WASMSection::Type, content.getBuffer());
    }
    
    void writeImportSection(WASMBinaryWriter& writer) {
        WASMBinaryWriter content;
        content.writeU32LEB(static_cast<uint32_t>(imports_.size()));
        for (const auto& imp : imports_) {
            content.writeString(imp.module);
            content.writeString(imp.name);
            content.writeByte(static_cast<uint8_t>(imp.kind));
            if (imp.kind == ImportDef::Function) {
                content.writeU32LEB(imp.typeIndex);
            }
        }
        writeSection(writer, WASMSection::Import, content.getBuffer());
    }
    
    void writeFunctionSection(WASMBinaryWriter& writer) {
        WASMBinaryWriter content;
        content.writeU32LEB(static_cast<uint32_t>(functions_.size()));
        for (const auto& func : functions_) {
            content.writeU32LEB(func.typeIndex);
        }
        writeSection(writer, WASMSection::Function, content.getBuffer());
    }
    
    void writeMemorySection(WASMBinaryWriter& writer) {
        WASMBinaryWriter content;
        content.writeU32LEB(1); // One memory
        if (memoryHasMax_) {
            content.writeByte(0x01); // Has max
            content.writeU32LEB(memoryInitial_);
            content.writeU32LEB(memoryMaximum_);
        } else {
            content.writeByte(0x00); // No max
            content.writeU32LEB(memoryInitial_);
        }
        writeSection(writer, WASMSection::Memory, content.getBuffer());
    }
    
    void writeGlobalSection(WASMBinaryWriter& writer) {
        WASMBinaryWriter content;
        content.writeU32LEB(static_cast<uint32_t>(globals_.size()));
        for (const auto& global : globals_) {
            content.writeValueType(global.type);
            content.writeByte(global.mutable_ ? 0x01 : 0x00);
            content.writeBytes(global.initExpr);
        }
        writeSection(writer, WASMSection::Global, content.getBuffer());
    }
    
    void writeExportSection(WASMBinaryWriter& writer) {
        WASMBinaryWriter content;
        
        // Count exports
        uint32_t numExports = memoryExported_ ? 1 : 0;
        for (const auto& func : functions_) {
            if (func.exported) numExports++;
        }
        for (const auto& global : globals_) {
            if (!global.exportName.empty()) numExports++;
        }
        
        content.writeU32LEB(numExports);
        
        // Memory export
        if (memoryExported_) {
            content.writeString(memoryExportName_);
            content.writeByte(0x02); // Memory
            content.writeU32LEB(0);
        }
        
        // Function exports
        uint32_t funcIdx = numImportedFunctions_;
        for (const auto& func : functions_) {
            if (func.exported) {
                content.writeString(func.name);
                content.writeByte(0x00); // Function
                content.writeU32LEB(funcIdx);
            }
            funcIdx++;
        }
        
        // Global exports
        for (uint32_t i = 0; i < globals_.size(); ++i) {
            if (!globals_[i].exportName.empty()) {
                content.writeString(globals_[i].exportName);
                content.writeByte(0x03); // Global
                content.writeU32LEB(i);
            }
        }
        
        writeSection(writer, WASMSection::Export, content.getBuffer());
    }
    
    void writeStartSection(WASMBinaryWriter& writer) {
        WASMBinaryWriter content;
        content.writeU32LEB(startFunction_);
        writeSection(writer, WASMSection::Start, content.getBuffer());
    }
    
    void writeDataCountSection(WASMBinaryWriter& writer) {
        WASMBinaryWriter content;
        content.writeU32LEB(static_cast<uint32_t>(dataSegments_.size()));
        writeSection(writer, WASMSection::DataCount, content.getBuffer());
    }
    
    void writeCodeSection(WASMBinaryWriter& writer) {
        WASMBinaryWriter content;
        content.writeU32LEB(static_cast<uint32_t>(functions_.size()));
        
        for (const auto& func : functions_) {
            WASMBinaryWriter funcBody;
            
            // Locals
            // Group consecutive locals of same type
            std::vector<std::pair<uint32_t, ValueType>> localGroups;
            if (!func.locals.empty()) {
                ValueType currentType = func.locals[0];
                uint32_t count = 1;
                for (size_t i = 1; i < func.locals.size(); ++i) {
                    if (func.locals[i] == currentType) {
                        count++;
                    } else {
                        localGroups.push_back({count, currentType});
                        currentType = func.locals[i];
                        count = 1;
                    }
                }
                localGroups.push_back({count, currentType});
            }
            
            funcBody.writeU32LEB(static_cast<uint32_t>(localGroups.size()));
            for (const auto& [count, type] : localGroups) {
                funcBody.writeU32LEB(count);
                funcBody.writeValueType(type);
            }
            
            // Code
            funcBody.writeBytes(func.code);
            
            // Write function body with size
            content.writeU32LEB(static_cast<uint32_t>(funcBody.size()));
            content.writeBytes(funcBody.getBuffer());
        }
        
        writeSection(writer, WASMSection::Code, content.getBuffer());
    }
    
    void writeDataSection(WASMBinaryWriter& writer) {
        WASMBinaryWriter content;
        content.writeU32LEB(static_cast<uint32_t>(dataSegments_.size()));
        
        for (const auto& seg : dataSegments_) {
            content.writeByte(0x00); // Active segment, memory 0
            content.writeBytes(seg.offsetExpr);
            content.writeU32LEB(static_cast<uint32_t>(seg.data.size()));
            content.writeBytes(seg.data);
        }
        
        writeSection(writer, WASMSection::Data, content.getBuffer());
    }
    
    std::vector<FunctionType> types_;
    std::vector<ImportDef> imports_;
    std::vector<FunctionDef> functions_;
    std::vector<GlobalDef> globals_;
    std::vector<DataSegment> dataSegments_;
    
    uint32_t memoryInitial_ = 1;
    uint32_t memoryMaximum_ = 0;
    bool memoryHasMax_ = false;
    bool memoryExported_ = true;
    std::string memoryExportName_ = "memory";
    
    uint32_t numImportedFunctions_ = 0;
    uint32_t startFunction_ = 0;
    bool hasStartFunction_ = false;
};

/**
 * @brief High-level WASM code generator from IR
 */
class WASMCodeGenerator {
public:
    struct CompilationResult {
        bool success = false;
        std::vector<uint8_t> binary;
        std::string wat;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
    };
    
    WASMCodeGenerator() = default;
    
    // Configure generator
    void setOptimizationLevel(int level) { optimizationLevel_ = level; }
    void setDebugInfo(bool enable) { debugInfo_ = enable; }
    void setMemoryPages(uint32_t initial, uint32_t max = 0) {
        memoryInitial_ = initial;
        memoryMax_ = max;
    }
    
    // Generate a simple function
    CompilationResult compileFunction(
        const std::string& name,
        const std::vector<ValueType>& params,
        const std::vector<ValueType>& results,
        std::function<void(WASMInstructionBuilder&)> bodyBuilder)
    {
        CompilationResult result;
        
        try {
            WASMModuleBuilder module;
            
            // Add function type
            FunctionType funcType;
            funcType.params = params;
            funcType.results = results;
            uint32_t typeIdx = module.addType(funcType);
            
            // Build function body
            WASMInstructionBuilder builder;
            bodyBuilder(builder);
            builder.end(); // End function
            
            // Add function
            WASMModuleBuilder::FunctionDef func;
            func.name = name;
            func.typeIndex = typeIdx;
            func.code = builder.getCode();
            func.exported = true;
            module.addFunction(func);
            
            // Set memory
            module.setMemory(memoryInitial_, memoryMax_, memoryMax_ > 0);
            
            // Build module
            result.binary = module.build();
            result.wat = module.toWAT();
            result.success = true;
            
        } catch (const std::exception& e) {
            result.errors.push_back(e.what());
        }
        
        return result;
    }
    
    // Create a complete module with multiple functions
    CompilationResult compileModule(
        const std::vector<std::tuple<std::string, FunctionType, std::vector<uint8_t>>>& functions)
    {
        CompilationResult result;
        
        try {
            WASMModuleBuilder module;
            
            for (const auto& [name, type, code] : functions) {
                uint32_t typeIdx = module.addType(type);
                
                WASMModuleBuilder::FunctionDef func;
                func.name = name;
                func.typeIndex = typeIdx;
                func.code = code;
                func.exported = true;
                module.addFunction(func);
            }
            
            module.setMemory(memoryInitial_, memoryMax_, memoryMax_ > 0);
            
            result.binary = module.build();
            result.wat = module.toWAT();
            result.success = true;
            
        } catch (const std::exception& e) {
            result.errors.push_back(e.what());
        }
        
        return result;
    }
    
private:
    int optimizationLevel_ = 0;
    bool debugInfo_ = false;
    uint32_t memoryInitial_ = 1;
    uint32_t memoryMax_ = 0;
};

} // namespace WASM
} // namespace Compiler
} // namespace RawrXD
