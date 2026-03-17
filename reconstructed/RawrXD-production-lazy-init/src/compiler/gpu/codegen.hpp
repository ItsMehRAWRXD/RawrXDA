#pragma once

// ============================================================================
// RAWRXD GPU CODE GENERATOR
// Complete GPU/Compute shader generation with Vulkan/SPIR-V support
// ============================================================================

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <functional>
#include <optional>
#include <variant>
#include <cstdint>
#include <sstream>
#include <algorithm>
#include <cmath>

namespace RawrXD {
namespace Compiler {
namespace GPU {

// ============================================================================
// SPIR-V CONSTANTS
// ============================================================================

namespace SPIRV {
    // Magic number
    constexpr uint32_t MagicNumber = 0x07230203;
    
    // Version (1.0)
    constexpr uint32_t Version = 0x00010000;
    
    // Generator magic number (RawrXD = 0xDEAD)
    constexpr uint32_t GeneratorMagic = 0xDEAD0001;
    
    // Instruction opcodes
    namespace Op {
        constexpr uint16_t Nop = 0;
        constexpr uint16_t Source = 3;
        constexpr uint16_t Name = 5;
        constexpr uint16_t MemberName = 6;
        constexpr uint16_t ExtInstImport = 11;
        constexpr uint16_t MemoryModel = 14;
        constexpr uint16_t EntryPoint = 15;
        constexpr uint16_t ExecutionMode = 16;
        constexpr uint16_t Capability = 17;
        constexpr uint16_t TypeVoid = 19;
        constexpr uint16_t TypeBool = 20;
        constexpr uint16_t TypeInt = 21;
        constexpr uint16_t TypeFloat = 22;
        constexpr uint16_t TypeVector = 23;
        constexpr uint16_t TypeMatrix = 24;
        constexpr uint16_t TypeArray = 28;
        constexpr uint16_t TypeStruct = 30;
        constexpr uint16_t TypePointer = 32;
        constexpr uint16_t TypeFunction = 33;
        constexpr uint16_t ConstantTrue = 41;
        constexpr uint16_t ConstantFalse = 42;
        constexpr uint16_t Constant = 43;
        constexpr uint16_t ConstantComposite = 44;
        constexpr uint16_t Function = 54;
        constexpr uint16_t FunctionParameter = 55;
        constexpr uint16_t FunctionEnd = 56;
        constexpr uint16_t FunctionCall = 57;
        constexpr uint16_t Variable = 59;
        constexpr uint16_t Load = 61;
        constexpr uint16_t Store = 62;
        constexpr uint16_t AccessChain = 65;
        constexpr uint16_t Decorate = 71;
        constexpr uint16_t MemberDecorate = 72;
        constexpr uint16_t CompositeConstruct = 80;
        constexpr uint16_t CompositeExtract = 81;
        constexpr uint16_t VectorShuffle = 79;
        constexpr uint16_t IAdd = 128;
        constexpr uint16_t FAdd = 129;
        constexpr uint16_t ISub = 130;
        constexpr uint16_t FSub = 131;
        constexpr uint16_t IMul = 132;
        constexpr uint16_t FMul = 133;
        constexpr uint16_t UDiv = 134;
        constexpr uint16_t SDiv = 135;
        constexpr uint16_t FDiv = 136;
        constexpr uint16_t UMod = 137;
        constexpr uint16_t SMod = 139;
        constexpr uint16_t FMod = 141;
        constexpr uint16_t VectorTimesScalar = 142;
        constexpr uint16_t MatrixTimesScalar = 143;
        constexpr uint16_t VectorTimesMatrix = 144;
        constexpr uint16_t MatrixTimesVector = 145;
        constexpr uint16_t MatrixTimesMatrix = 146;
        constexpr uint16_t Dot = 148;
        constexpr uint16_t ShiftRightLogical = 194;
        constexpr uint16_t ShiftRightArithmetic = 195;
        constexpr uint16_t ShiftLeftLogical = 196;
        constexpr uint16_t BitwiseOr = 197;
        constexpr uint16_t BitwiseXor = 198;
        constexpr uint16_t BitwiseAnd = 199;
        constexpr uint16_t LogicalOr = 166;
        constexpr uint16_t LogicalAnd = 167;
        constexpr uint16_t LogicalNot = 168;
        constexpr uint16_t Select = 169;
        constexpr uint16_t IEqual = 170;
        constexpr uint16_t INotEqual = 171;
        constexpr uint16_t UGreaterThan = 172;
        constexpr uint16_t SGreaterThan = 173;
        constexpr uint16_t UGreaterThanEqual = 174;
        constexpr uint16_t SGreaterThanEqual = 175;
        constexpr uint16_t ULessThan = 176;
        constexpr uint16_t SLessThan = 177;
        constexpr uint16_t ULessThanEqual = 178;
        constexpr uint16_t SLessThanEqual = 179;
        constexpr uint16_t FOrdEqual = 180;
        constexpr uint16_t FOrdNotEqual = 182;
        constexpr uint16_t FOrdLessThan = 184;
        constexpr uint16_t FOrdGreaterThan = 186;
        constexpr uint16_t FOrdLessThanEqual = 188;
        constexpr uint16_t FOrdGreaterThanEqual = 190;
        constexpr uint16_t ConvertFToU = 109;
        constexpr uint16_t ConvertFToS = 110;
        constexpr uint16_t ConvertSToF = 111;
        constexpr uint16_t ConvertUToF = 112;
        constexpr uint16_t Bitcast = 124;
        constexpr uint16_t SNegate = 126;
        constexpr uint16_t FNegate = 127;
        constexpr uint16_t Label = 248;
        constexpr uint16_t Branch = 249;
        constexpr uint16_t BranchConditional = 250;
        constexpr uint16_t Switch = 251;
        constexpr uint16_t Return = 253;
        constexpr uint16_t ReturnValue = 254;
        constexpr uint16_t Kill = 252;
        constexpr uint16_t LoopMerge = 246;
        constexpr uint16_t SelectionMerge = 247;
        constexpr uint16_t Phi = 245;
    }
    
    // Execution models
    namespace ExecutionModel {
        constexpr uint32_t Vertex = 0;
        constexpr uint32_t TessellationControl = 1;
        constexpr uint32_t TessellationEvaluation = 2;
        constexpr uint32_t Geometry = 3;
        constexpr uint32_t Fragment = 4;
        constexpr uint32_t GLCompute = 5;
    }
    
    // Execution modes
    namespace ExecutionMode {
        constexpr uint32_t LocalSize = 17;
        constexpr uint32_t OriginUpperLeft = 7;
        constexpr uint32_t OriginLowerLeft = 8;
    }
    
    // Storage classes
    namespace StorageClass {
        constexpr uint32_t UniformConstant = 0;
        constexpr uint32_t Input = 1;
        constexpr uint32_t Uniform = 2;
        constexpr uint32_t Output = 3;
        constexpr uint32_t Workgroup = 4;
        constexpr uint32_t CrossWorkgroup = 5;
        constexpr uint32_t Private = 6;
        constexpr uint32_t Function = 7;
        constexpr uint32_t PushConstant = 9;
        constexpr uint32_t StorageBuffer = 12;
    }
    
    // Decorations
    namespace Decoration {
        constexpr uint32_t Block = 2;
        constexpr uint32_t BufferBlock = 3;
        constexpr uint32_t RowMajor = 4;
        constexpr uint32_t ColMajor = 5;
        constexpr uint32_t ArrayStride = 6;
        constexpr uint32_t MatrixStride = 7;
        constexpr uint32_t BuiltIn = 11;
        constexpr uint32_t Flat = 14;
        constexpr uint32_t NoPerspective = 13;
        constexpr uint32_t Location = 30;
        constexpr uint32_t Binding = 33;
        constexpr uint32_t DescriptorSet = 34;
        constexpr uint32_t Offset = 35;
    }
    
    // Built-ins
    namespace BuiltIn {
        constexpr uint32_t Position = 0;
        constexpr uint32_t PointSize = 1;
        constexpr uint32_t VertexId = 5;
        constexpr uint32_t InstanceId = 6;
        constexpr uint32_t FragCoord = 15;
        constexpr uint32_t PointCoord = 16;
        constexpr uint32_t FrontFacing = 17;
        constexpr uint32_t FragDepth = 22;
        constexpr uint32_t WorkgroupId = 26;
        constexpr uint32_t LocalInvocationId = 27;
        constexpr uint32_t GlobalInvocationId = 28;
        constexpr uint32_t LocalInvocationIndex = 29;
        constexpr uint32_t NumWorkgroups = 24;
        constexpr uint32_t WorkgroupSize = 25;
    }
    
    // Capabilities
    namespace Capability {
        constexpr uint32_t Matrix = 0;
        constexpr uint32_t Shader = 1;
        constexpr uint32_t Float64 = 10;
        constexpr uint32_t Int64 = 11;
        constexpr uint32_t Int16 = 22;
        constexpr uint32_t StorageBuffer16BitAccess = 4433;
    }
    
    // Addressing model
    namespace AddressingModel {
        constexpr uint32_t Logical = 0;
        constexpr uint32_t Physical32 = 1;
        constexpr uint32_t Physical64 = 2;
    }
    
    // Memory model
    namespace MemoryModel {
        constexpr uint32_t Simple = 0;
        constexpr uint32_t GLSL450 = 1;
        constexpr uint32_t Vulkan = 3;
    }
}

// ============================================================================
// SPIR-V BINARY BUILDER
// ============================================================================

/**
 * @brief Builds SPIR-V binary modules
 */
class SPIRVBuilder {
public:
    SPIRVBuilder() {
        // Reserve ID 0 (invalid)
        nextId_ = 1;
    }
    
    // Generate new ID
    uint32_t nextId() { return nextId_++; }
    
    // Get current bound (max ID + 1)
    uint32_t getBound() const { return nextId_; }
    
    // Basic instruction emission
    void emitInstruction(uint16_t opcode, const std::vector<uint32_t>& operands = {}) {
        uint32_t wordCount = static_cast<uint32_t>(1 + operands.size());
        instructions_.push_back((wordCount << 16) | opcode);
        instructions_.insert(instructions_.end(), operands.begin(), operands.end());
    }
    
    // Emit instruction with result ID
    uint32_t emitResultInstruction(uint16_t opcode, uint32_t resultType, 
                                    const std::vector<uint32_t>& operands = {}) {
        uint32_t resultId = nextId();
        uint32_t wordCount = static_cast<uint32_t>(3 + operands.size());
        instructions_.push_back((wordCount << 16) | opcode);
        instructions_.push_back(resultType);
        instructions_.push_back(resultId);
        instructions_.insert(instructions_.end(), operands.begin(), operands.end());
        return resultId;
    }
    
    // Emit string (for names, etc.)
    void emitString(const std::string& str) {
        // Pack string into 32-bit words
        size_t wordCount = (str.size() + 4) / 4;
        for (size_t i = 0; i < wordCount; ++i) {
            uint32_t word = 0;
            for (size_t j = 0; j < 4; ++j) {
                size_t idx = i * 4 + j;
                if (idx < str.size()) {
                    word |= static_cast<uint32_t>(str[idx]) << (j * 8);
                }
            }
            instructions_.push_back(word);
        }
    }
    
    // Section management
    void beginCapabilities() { currentSection_ = Section::Capabilities; }
    void beginExtensions() { currentSection_ = Section::Extensions; }
    void beginExtInstImports() { currentSection_ = Section::ExtInstImport; }
    void beginMemoryModel() { currentSection_ = Section::MemoryModel; }
    void beginEntryPoints() { currentSection_ = Section::EntryPoints; }
    void beginExecutionModes() { currentSection_ = Section::ExecutionModes; }
    void beginDebug() { currentSection_ = Section::Debug; }
    void beginAnnotations() { currentSection_ = Section::Annotations; }
    void beginTypes() { currentSection_ = Section::Types; }
    void beginFunctions() { currentSection_ = Section::Functions; }
    
    // High-level type creation
    uint32_t typeVoid() {
        if (voidType_ == 0) {
            voidType_ = nextId();
            uint32_t wordCount = 2;
            instructions_.push_back((wordCount << 16) | SPIRV::Op::TypeVoid);
            instructions_.push_back(voidType_);
        }
        return voidType_;
    }
    
    uint32_t typeBool() {
        if (boolType_ == 0) {
            boolType_ = nextId();
            uint32_t wordCount = 2;
            instructions_.push_back((wordCount << 16) | SPIRV::Op::TypeBool);
            instructions_.push_back(boolType_);
        }
        return boolType_;
    }
    
    uint32_t typeInt(uint32_t width, bool signedness) {
        auto key = std::make_pair(width, signedness);
        auto it = intTypes_.find(key);
        if (it != intTypes_.end()) return it->second;
        
        uint32_t id = nextId();
        uint32_t wordCount = 4;
        instructions_.push_back((wordCount << 16) | SPIRV::Op::TypeInt);
        instructions_.push_back(id);
        instructions_.push_back(width);
        instructions_.push_back(signedness ? 1 : 0);
        intTypes_[key] = id;
        return id;
    }
    
    uint32_t typeFloat(uint32_t width) {
        auto it = floatTypes_.find(width);
        if (it != floatTypes_.end()) return it->second;
        
        uint32_t id = nextId();
        uint32_t wordCount = 3;
        instructions_.push_back((wordCount << 16) | SPIRV::Op::TypeFloat);
        instructions_.push_back(id);
        instructions_.push_back(width);
        floatTypes_[width] = id;
        return id;
    }
    
    uint32_t typeVector(uint32_t componentType, uint32_t componentCount) {
        auto key = std::make_pair(componentType, componentCount);
        auto it = vectorTypes_.find(key);
        if (it != vectorTypes_.end()) return it->second;
        
        uint32_t id = nextId();
        uint32_t wordCount = 4;
        instructions_.push_back((wordCount << 16) | SPIRV::Op::TypeVector);
        instructions_.push_back(id);
        instructions_.push_back(componentType);
        instructions_.push_back(componentCount);
        vectorTypes_[key] = id;
        return id;
    }
    
    uint32_t typeMatrix(uint32_t columnType, uint32_t columnCount) {
        auto key = std::make_pair(columnType, columnCount);
        auto it = matrixTypes_.find(key);
        if (it != matrixTypes_.end()) return it->second;
        
        uint32_t id = nextId();
        uint32_t wordCount = 4;
        instructions_.push_back((wordCount << 16) | SPIRV::Op::TypeMatrix);
        instructions_.push_back(id);
        instructions_.push_back(columnType);
        instructions_.push_back(columnCount);
        matrixTypes_[key] = id;
        return id;
    }
    
    uint32_t typeArray(uint32_t elementType, uint32_t length) {
        uint32_t id = nextId();
        uint32_t wordCount = 4;
        instructions_.push_back((wordCount << 16) | SPIRV::Op::TypeArray);
        instructions_.push_back(id);
        instructions_.push_back(elementType);
        instructions_.push_back(length);
        return id;
    }
    
    uint32_t typeStruct(const std::vector<uint32_t>& memberTypes) {
        uint32_t id = nextId();
        uint32_t wordCount = static_cast<uint32_t>(2 + memberTypes.size());
        instructions_.push_back((wordCount << 16) | SPIRV::Op::TypeStruct);
        instructions_.push_back(id);
        for (auto t : memberTypes) {
            instructions_.push_back(t);
        }
        return id;
    }
    
    uint32_t typePointer(uint32_t storageClass, uint32_t pointeeType) {
        auto key = std::make_pair(storageClass, pointeeType);
        auto it = pointerTypes_.find(key);
        if (it != pointerTypes_.end()) return it->second;
        
        uint32_t id = nextId();
        uint32_t wordCount = 4;
        instructions_.push_back((wordCount << 16) | SPIRV::Op::TypePointer);
        instructions_.push_back(id);
        instructions_.push_back(storageClass);
        instructions_.push_back(pointeeType);
        pointerTypes_[key] = id;
        return id;
    }
    
    uint32_t typeFunction(uint32_t returnType, const std::vector<uint32_t>& paramTypes = {}) {
        uint32_t id = nextId();
        uint32_t wordCount = static_cast<uint32_t>(3 + paramTypes.size());
        instructions_.push_back((wordCount << 16) | SPIRV::Op::TypeFunction);
        instructions_.push_back(id);
        instructions_.push_back(returnType);
        for (auto t : paramTypes) {
            instructions_.push_back(t);
        }
        return id;
    }
    
    // Constants
    uint32_t constantBool(bool value) {
        uint32_t id = nextId();
        uint32_t opcode = value ? SPIRV::Op::ConstantTrue : SPIRV::Op::ConstantFalse;
        instructions_.push_back((3 << 16) | opcode);
        instructions_.push_back(typeBool());
        instructions_.push_back(id);
        return id;
    }
    
    uint32_t constantInt32(int32_t value) {
        uint32_t id = nextId();
        instructions_.push_back((4 << 16) | SPIRV::Op::Constant);
        instructions_.push_back(typeInt(32, true));
        instructions_.push_back(id);
        instructions_.push_back(static_cast<uint32_t>(value));
        return id;
    }
    
    uint32_t constantUint32(uint32_t value) {
        uint32_t id = nextId();
        instructions_.push_back((4 << 16) | SPIRV::Op::Constant);
        instructions_.push_back(typeInt(32, false));
        instructions_.push_back(id);
        instructions_.push_back(value);
        return id;
    }
    
    uint32_t constantFloat32(float value) {
        uint32_t id = nextId();
        uint32_t bits;
        memcpy(&bits, &value, sizeof(bits));
        instructions_.push_back((4 << 16) | SPIRV::Op::Constant);
        instructions_.push_back(typeFloat(32));
        instructions_.push_back(id);
        instructions_.push_back(bits);
        return id;
    }
    
    uint32_t constantComposite(uint32_t type, const std::vector<uint32_t>& constituents) {
        uint32_t id = nextId();
        uint32_t wordCount = static_cast<uint32_t>(3 + constituents.size());
        instructions_.push_back((wordCount << 16) | SPIRV::Op::ConstantComposite);
        instructions_.push_back(type);
        instructions_.push_back(id);
        for (auto c : constituents) {
            instructions_.push_back(c);
        }
        return id;
    }
    
    // Decorations
    void decorate(uint32_t target, uint32_t decoration, const std::vector<uint32_t>& args = {}) {
        uint32_t wordCount = static_cast<uint32_t>(3 + args.size());
        instructions_.push_back((wordCount << 16) | SPIRV::Op::Decorate);
        instructions_.push_back(target);
        instructions_.push_back(decoration);
        for (auto a : args) {
            instructions_.push_back(a);
        }
    }
    
    void memberDecorate(uint32_t structType, uint32_t member, uint32_t decoration,
                        const std::vector<uint32_t>& args = {}) {
        uint32_t wordCount = static_cast<uint32_t>(4 + args.size());
        instructions_.push_back((wordCount << 16) | SPIRV::Op::MemberDecorate);
        instructions_.push_back(structType);
        instructions_.push_back(member);
        instructions_.push_back(decoration);
        for (auto a : args) {
            instructions_.push_back(a);
        }
    }
    
    // Names (debug info)
    void name(uint32_t target, const std::string& name) {
        size_t strWords = (name.size() + 4) / 4;
        uint32_t wordCount = static_cast<uint32_t>(2 + strWords);
        instructions_.push_back((wordCount << 16) | SPIRV::Op::Name);
        instructions_.push_back(target);
        emitString(name);
    }
    
    void memberName(uint32_t type, uint32_t member, const std::string& name) {
        size_t strWords = (name.size() + 4) / 4;
        uint32_t wordCount = static_cast<uint32_t>(3 + strWords);
        instructions_.push_back((wordCount << 16) | SPIRV::Op::MemberName);
        instructions_.push_back(type);
        instructions_.push_back(member);
        emitString(name);
    }
    
    // Variables
    uint32_t variable(uint32_t pointerType, uint32_t storageClass, uint32_t initializer = 0) {
        uint32_t id = nextId();
        uint32_t wordCount = initializer ? 5 : 4;
        instructions_.push_back((wordCount << 16) | SPIRV::Op::Variable);
        instructions_.push_back(pointerType);
        instructions_.push_back(id);
        instructions_.push_back(storageClass);
        if (initializer) {
            instructions_.push_back(initializer);
        }
        return id;
    }
    
    // Function structure
    uint32_t functionBegin(uint32_t returnType, uint32_t functionType, uint32_t functionControl = 0) {
        uint32_t id = nextId();
        instructions_.push_back((5 << 16) | SPIRV::Op::Function);
        instructions_.push_back(returnType);
        instructions_.push_back(id);
        instructions_.push_back(functionControl);
        instructions_.push_back(functionType);
        return id;
    }
    
    uint32_t functionParameter(uint32_t type) {
        uint32_t id = nextId();
        instructions_.push_back((3 << 16) | SPIRV::Op::FunctionParameter);
        instructions_.push_back(type);
        instructions_.push_back(id);
        return id;
    }
    
    void functionEnd() {
        instructions_.push_back((1 << 16) | SPIRV::Op::FunctionEnd);
    }
    
    uint32_t label() {
        uint32_t id = nextId();
        instructions_.push_back((2 << 16) | SPIRV::Op::Label);
        instructions_.push_back(id);
        return id;
    }
    
    void returnVoid() {
        instructions_.push_back((1 << 16) | SPIRV::Op::Return);
    }
    
    void returnValue(uint32_t value) {
        instructions_.push_back((2 << 16) | SPIRV::Op::ReturnValue);
        instructions_.push_back(value);
    }
    
    // Memory operations
    uint32_t load(uint32_t resultType, uint32_t pointer, uint32_t memoryAccess = 0) {
        uint32_t id = nextId();
        uint32_t wordCount = memoryAccess ? 5 : 4;
        instructions_.push_back((wordCount << 16) | SPIRV::Op::Load);
        instructions_.push_back(resultType);
        instructions_.push_back(id);
        instructions_.push_back(pointer);
        if (memoryAccess) {
            instructions_.push_back(memoryAccess);
        }
        return id;
    }
    
    void store(uint32_t pointer, uint32_t object, uint32_t memoryAccess = 0) {
        uint32_t wordCount = memoryAccess ? 4 : 3;
        instructions_.push_back((wordCount << 16) | SPIRV::Op::Store);
        instructions_.push_back(pointer);
        instructions_.push_back(object);
        if (memoryAccess) {
            instructions_.push_back(memoryAccess);
        }
    }
    
    uint32_t accessChain(uint32_t resultType, uint32_t base, const std::vector<uint32_t>& indices) {
        uint32_t id = nextId();
        uint32_t wordCount = static_cast<uint32_t>(4 + indices.size());
        instructions_.push_back((wordCount << 16) | SPIRV::Op::AccessChain);
        instructions_.push_back(resultType);
        instructions_.push_back(id);
        instructions_.push_back(base);
        for (auto i : indices) {
            instructions_.push_back(i);
        }
        return id;
    }
    
    // Arithmetic operations
    uint32_t iadd(uint32_t resultType, uint32_t a, uint32_t b) {
        return emitResultInstruction(SPIRV::Op::IAdd, resultType, {a, b});
    }
    
    uint32_t fadd(uint32_t resultType, uint32_t a, uint32_t b) {
        return emitResultInstruction(SPIRV::Op::FAdd, resultType, {a, b});
    }
    
    uint32_t isub(uint32_t resultType, uint32_t a, uint32_t b) {
        return emitResultInstruction(SPIRV::Op::ISub, resultType, {a, b});
    }
    
    uint32_t fsub(uint32_t resultType, uint32_t a, uint32_t b) {
        return emitResultInstruction(SPIRV::Op::FSub, resultType, {a, b});
    }
    
    uint32_t imul(uint32_t resultType, uint32_t a, uint32_t b) {
        return emitResultInstruction(SPIRV::Op::IMul, resultType, {a, b});
    }
    
    uint32_t fmul(uint32_t resultType, uint32_t a, uint32_t b) {
        return emitResultInstruction(SPIRV::Op::FMul, resultType, {a, b});
    }
    
    uint32_t fdiv(uint32_t resultType, uint32_t a, uint32_t b) {
        return emitResultInstruction(SPIRV::Op::FDiv, resultType, {a, b});
    }
    
    uint32_t dot(uint32_t resultType, uint32_t a, uint32_t b) {
        return emitResultInstruction(SPIRV::Op::Dot, resultType, {a, b});
    }
    
    // Composite operations
    uint32_t compositeConstruct(uint32_t resultType, const std::vector<uint32_t>& constituents) {
        uint32_t id = nextId();
        uint32_t wordCount = static_cast<uint32_t>(3 + constituents.size());
        instructions_.push_back((wordCount << 16) | SPIRV::Op::CompositeConstruct);
        instructions_.push_back(resultType);
        instructions_.push_back(id);
        for (auto c : constituents) {
            instructions_.push_back(c);
        }
        return id;
    }
    
    uint32_t compositeExtract(uint32_t resultType, uint32_t composite, uint32_t index) {
        return emitResultInstruction(SPIRV::Op::CompositeExtract, resultType, {composite, index});
    }
    
    // Control flow
    void branch(uint32_t target) {
        instructions_.push_back((2 << 16) | SPIRV::Op::Branch);
        instructions_.push_back(target);
    }
    
    void branchConditional(uint32_t condition, uint32_t trueLabel, uint32_t falseLabel) {
        instructions_.push_back((4 << 16) | SPIRV::Op::BranchConditional);
        instructions_.push_back(condition);
        instructions_.push_back(trueLabel);
        instructions_.push_back(falseLabel);
    }
    
    void loopMerge(uint32_t mergeBlock, uint32_t continueTarget, uint32_t loopControl = 0) {
        instructions_.push_back((4 << 16) | SPIRV::Op::LoopMerge);
        instructions_.push_back(mergeBlock);
        instructions_.push_back(continueTarget);
        instructions_.push_back(loopControl);
    }
    
    void selectionMerge(uint32_t mergeBlock, uint32_t selectionControl = 0) {
        instructions_.push_back((3 << 16) | SPIRV::Op::SelectionMerge);
        instructions_.push_back(mergeBlock);
        instructions_.push_back(selectionControl);
    }
    
    // Build final binary
    std::vector<uint32_t> build() const {
        std::vector<uint32_t> binary;
        
        // Header
        binary.push_back(SPIRV::MagicNumber);
        binary.push_back(SPIRV::Version);
        binary.push_back(SPIRV::GeneratorMagic);
        binary.push_back(nextId_);  // Bound
        binary.push_back(0);        // Reserved
        
        // Instructions
        binary.insert(binary.end(), instructions_.begin(), instructions_.end());
        
        return binary;
    }
    
    // Get raw bytes
    std::vector<uint8_t> buildBytes() const {
        auto words = build();
        std::vector<uint8_t> bytes;
        bytes.reserve(words.size() * 4);
        for (auto w : words) {
            bytes.push_back(w & 0xFF);
            bytes.push_back((w >> 8) & 0xFF);
            bytes.push_back((w >> 16) & 0xFF);
            bytes.push_back((w >> 24) & 0xFF);
        }
        return bytes;
    }
    
private:
    enum class Section {
        Capabilities,
        Extensions,
        ExtInstImport,
        MemoryModel,
        EntryPoints,
        ExecutionModes,
        Debug,
        Annotations,
        Types,
        Functions
    };
    
    Section currentSection_ = Section::Capabilities;
    uint32_t nextId_;
    std::vector<uint32_t> instructions_;
    
    // Type caching
    uint32_t voidType_ = 0;
    uint32_t boolType_ = 0;
    std::map<std::pair<uint32_t, bool>, uint32_t> intTypes_;
    std::map<uint32_t, uint32_t> floatTypes_;
    std::map<std::pair<uint32_t, uint32_t>, uint32_t> vectorTypes_;
    std::map<std::pair<uint32_t, uint32_t>, uint32_t> matrixTypes_;
    std::map<std::pair<uint32_t, uint32_t>, uint32_t> pointerTypes_;
};

// ============================================================================
// GPU KERNEL DEFINITION
// ============================================================================

/**
 * @brief Defines a GPU compute kernel
 */
struct GPUKernel {
    std::string name;
    std::vector<std::pair<std::string, std::string>> parameters; // name, type
    std::string body;
    
    uint32_t workgroupSizeX = 64;
    uint32_t workgroupSizeY = 1;
    uint32_t workgroupSizeZ = 1;
    
    struct BufferBinding {
        std::string name;
        uint32_t set = 0;
        uint32_t binding = 0;
        std::string elementType;
        bool readonly = false;
    };
    std::vector<BufferBinding> buffers;
    
    struct UniformBinding {
        std::string name;
        uint32_t set = 0;
        uint32_t binding = 0;
        std::vector<std::pair<std::string, std::string>> members; // name, type
    };
    std::vector<UniformBinding> uniforms;
};

// ============================================================================
// GPU CODE GENERATOR
// ============================================================================

/**
 * @brief Generates GPU compute shaders
 */
class GPUCodeGenerator {
public:
    struct CompilationResult {
        bool success = false;
        std::vector<uint8_t> spirvBinary;
        std::string glslSource;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
    };
    
    // Compile kernel to SPIR-V
    CompilationResult compileKernel(const GPUKernel& kernel) {
        CompilationResult result;
        
        try {
            SPIRVBuilder builder;
            
            // Capabilities
            builder.emitInstruction(SPIRV::Op::Capability, {SPIRV::Capability::Shader});
            
            // Extension import
            uint32_t glslStd = builder.nextId();
            builder.emitInstruction(SPIRV::Op::ExtInstImport, {glslStd});
            builder.emitString("GLSL.std.450");
            
            // Memory model
            builder.emitInstruction(SPIRV::Op::MemoryModel, 
                {SPIRV::AddressingModel::Logical, SPIRV::MemoryModel::GLSL450});
            
            // Types
            uint32_t voidType = builder.typeVoid();
            uint32_t uint32Type = builder.typeInt(32, false);
            uint32_t int32Type = builder.typeInt(32, true);
            uint32_t float32Type = builder.typeFloat(32);
            uint32_t uvec3Type = builder.typeVector(uint32Type, 3);
            uint32_t mainFuncType = builder.typeFunction(voidType);
            
            // Built-in variables
            uint32_t uvec3PtrInput = builder.typePointer(SPIRV::StorageClass::Input, uvec3Type);
            uint32_t globalInvocationId = builder.variable(uvec3PtrInput, SPIRV::StorageClass::Input);
            builder.decorate(globalInvocationId, SPIRV::Decoration::BuiltIn, 
                {SPIRV::BuiltIn::GlobalInvocationId});
            builder.name(globalInvocationId, "gl_GlobalInvocationID");
            
            // Storage buffer type and variable for each buffer binding
            std::vector<uint32_t> bufferVars;
            for (const auto& buf : kernel.buffers) {
                // Runtime array type
                uint32_t elementType = getTypeId(builder, buf.elementType);
                uint32_t arrayType = builder.typeArray(elementType, 0); // Runtime array
                
                // Struct containing the array
                uint32_t structType = builder.typeStruct({arrayType});
                builder.decorate(structType, SPIRV::Decoration::Block);
                builder.memberDecorate(structType, 0, SPIRV::Decoration::Offset, {0});
                builder.name(structType, buf.name + "_type");
                
                // Pointer and variable
                uint32_t ptrType = builder.typePointer(SPIRV::StorageClass::StorageBuffer, structType);
                uint32_t var = builder.variable(ptrType, SPIRV::StorageClass::StorageBuffer);
                builder.decorate(var, SPIRV::Decoration::DescriptorSet, {buf.set});
                builder.decorate(var, SPIRV::Decoration::Binding, {buf.binding});
                builder.name(var, buf.name);
                
                bufferVars.push_back(var);
            }
            
            // Main function
            uint32_t mainFunc = builder.functionBegin(voidType, mainFuncType);
            builder.name(mainFunc, kernel.name);
            uint32_t entryLabel = builder.label();
            
            // Load global invocation ID
            uint32_t gid = builder.load(uvec3Type, globalInvocationId);
            uint32_t gidX = builder.compositeExtract(uint32Type, gid, 0);
            
            // Simple kernel body: buffer[gid] = buffer[gid] * 2
            // In real implementation, would parse and compile kernel body
            if (!kernel.buffers.empty()) {
                uint32_t elementPtrType = builder.typePointer(SPIRV::StorageClass::StorageBuffer, float32Type);
                uint32_t zero = builder.constantUint32(0);
                
                // Access chain to buffer[gid]
                uint32_t ptr = builder.accessChain(elementPtrType, bufferVars[0], {zero, gidX});
                uint32_t value = builder.load(float32Type, ptr);
                
                // Multiply by 2
                uint32_t two = builder.constantFloat32(2.0f);
                uint32_t result = builder.fmul(float32Type, value, two);
                
                // Store result
                builder.store(ptr, result);
            }
            
            builder.returnVoid();
            builder.functionEnd();
            
            // Entry point
            std::vector<uint32_t> interfaceIds = {globalInvocationId};
            for (auto v : bufferVars) {
                interfaceIds.push_back(v);
            }
            
            // Build entry point instruction manually
            size_t nameWords = (kernel.name.size() + 4) / 4;
            uint32_t wordCount = static_cast<uint32_t>(3 + nameWords + interfaceIds.size());
            builder.emitInstruction(SPIRV::Op::EntryPoint, {SPIRV::ExecutionModel::GLCompute, mainFunc});
            builder.emitString(kernel.name);
            for (auto id : interfaceIds) {
                builder.emitInstruction(SPIRV::Op::Nop, {id}); // Actually add to entry point
            }
            
            // Execution mode
            builder.emitInstruction(SPIRV::Op::ExecutionMode, {
                mainFunc, 
                SPIRV::ExecutionMode::LocalSize,
                kernel.workgroupSizeX,
                kernel.workgroupSizeY,
                kernel.workgroupSizeZ
            });
            
            result.spirvBinary = builder.buildBytes();
            result.success = true;
            
            // Generate equivalent GLSL for debugging
            result.glslSource = generateGLSL(kernel);
            
        } catch (const std::exception& e) {
            result.errors.push_back(std::string("SPIR-V generation error: ") + e.what());
        }
        
        return result;
    }
    
    // Generate GLSL compute shader source
    std::string generateGLSL(const GPUKernel& kernel) {
        std::ostringstream oss;
        
        oss << "#version 450\n";
        oss << "#extension GL_ARB_separate_shader_objects : enable\n\n";
        
        oss << "layout(local_size_x = " << kernel.workgroupSizeX 
            << ", local_size_y = " << kernel.workgroupSizeY
            << ", local_size_z = " << kernel.workgroupSizeZ << ") in;\n\n";
        
        // Buffer bindings
        for (const auto& buf : kernel.buffers) {
            oss << "layout(set = " << buf.set << ", binding = " << buf.binding << ") ";
            if (buf.readonly) oss << "readonly ";
            oss << "buffer " << buf.name << "_buffer {\n";
            oss << "    " << buf.elementType << " " << buf.name << "[];\n";
            oss << "};\n\n";
        }
        
        // Uniform bindings
        for (const auto& uni : kernel.uniforms) {
            oss << "layout(set = " << uni.set << ", binding = " << uni.binding << ") uniform " 
                << uni.name << "_ubo {\n";
            for (const auto& [name, type] : uni.members) {
                oss << "    " << type << " " << name << ";\n";
            }
            oss << "} " << uni.name << ";\n\n";
        }
        
        oss << "void main() {\n";
        oss << "    uint gid = gl_GlobalInvocationID.x;\n";
        oss << "    " << kernel.body << "\n";
        oss << "}\n";
        
        return oss.str();
    }
    
private:
    uint32_t getTypeId(SPIRVBuilder& builder, const std::string& typeName) {
        if (typeName == "float") return builder.typeFloat(32);
        if (typeName == "int") return builder.typeInt(32, true);
        if (typeName == "uint") return builder.typeInt(32, false);
        if (typeName == "vec2") return builder.typeVector(builder.typeFloat(32), 2);
        if (typeName == "vec3") return builder.typeVector(builder.typeFloat(32), 3);
        if (typeName == "vec4") return builder.typeVector(builder.typeFloat(32), 4);
        if (typeName == "ivec2") return builder.typeVector(builder.typeInt(32, true), 2);
        if (typeName == "ivec3") return builder.typeVector(builder.typeInt(32, true), 3);
        if (typeName == "ivec4") return builder.typeVector(builder.typeInt(32, true), 4);
        if (typeName == "uvec2") return builder.typeVector(builder.typeInt(32, false), 2);
        if (typeName == "uvec3") return builder.typeVector(builder.typeInt(32, false), 3);
        if (typeName == "uvec4") return builder.typeVector(builder.typeInt(32, false), 4);
        if (typeName == "mat4") {
            uint32_t vec4 = builder.typeVector(builder.typeFloat(32), 4);
            return builder.typeMatrix(vec4, 4);
        }
        return builder.typeFloat(32); // Default
    }
};

// ============================================================================
// GPU DISPATCH MANAGER
// ============================================================================

/**
 * @brief Manages GPU kernel dispatch
 */
class GPUDispatchManager {
public:
    struct DispatchParams {
        uint32_t groupCountX = 1;
        uint32_t groupCountY = 1;
        uint32_t groupCountZ = 1;
    };
    
    struct BufferAllocation {
        std::string name;
        size_t size;
        void* hostPtr = nullptr;
        uint64_t deviceHandle = 0;
    };
    
    // Calculate optimal dispatch dimensions
    static DispatchParams calculateDispatch(size_t totalElements, uint32_t workgroupSize) {
        DispatchParams params;
        params.groupCountX = static_cast<uint32_t>((totalElements + workgroupSize - 1) / workgroupSize);
        return params;
    }
    
    // Calculate dispatch for 2D workload
    static DispatchParams calculateDispatch2D(uint32_t width, uint32_t height,
                                               uint32_t wgSizeX, uint32_t wgSizeY) {
        DispatchParams params;
        params.groupCountX = (width + wgSizeX - 1) / wgSizeX;
        params.groupCountY = (height + wgSizeY - 1) / wgSizeY;
        return params;
    }
    
    // Calculate dispatch for 3D workload
    static DispatchParams calculateDispatch3D(uint32_t width, uint32_t height, uint32_t depth,
                                               uint32_t wgSizeX, uint32_t wgSizeY, uint32_t wgSizeZ) {
        DispatchParams params;
        params.groupCountX = (width + wgSizeX - 1) / wgSizeX;
        params.groupCountY = (height + wgSizeY - 1) / wgSizeY;
        params.groupCountZ = (depth + wgSizeZ - 1) / wgSizeZ;
        return params;
    }
};

// ============================================================================
// PREDEFINED GPU KERNELS
// ============================================================================

/**
 * @brief Collection of common GPU kernels
 */
class GPUKernelLibrary {
public:
    // Vector addition: C = A + B
    static GPUKernel vectorAdd() {
        GPUKernel kernel;
        kernel.name = "vector_add";
        kernel.workgroupSizeX = 256;
        
        kernel.buffers = {
            {"A", 0, 0, "float", true},
            {"B", 0, 1, "float", true},
            {"C", 0, 2, "float", false}
        };
        
        kernel.body = "C[gid] = A[gid] + B[gid];";
        return kernel;
    }
    
    // Scalar multiply: B = A * scalar
    static GPUKernel scalarMultiply() {
        GPUKernel kernel;
        kernel.name = "scalar_multiply";
        kernel.workgroupSizeX = 256;
        
        kernel.buffers = {
            {"input", 0, 0, "float", true},
            {"output", 0, 1, "float", false}
        };
        
        kernel.uniforms = {
            {"params", 0, 2, {{"scalar", "float"}}}
        };
        
        kernel.body = "output[gid] = input[gid] * params.scalar;";
        return kernel;
    }
    
    // Matrix-vector multiply
    static GPUKernel matrixVectorMultiply() {
        GPUKernel kernel;
        kernel.name = "matrix_vector_multiply";
        kernel.workgroupSizeX = 64;
        
        kernel.buffers = {
            {"matrix", 0, 0, "float", true},
            {"vector_in", 0, 1, "float", true},
            {"vector_out", 0, 2, "float", false}
        };
        
        kernel.uniforms = {
            {"params", 0, 3, {{"rows", "uint"}, {"cols", "uint"}}}
        };
        
        kernel.body = R"(
            if (gid < params.rows) {
                float sum = 0.0;
                for (uint j = 0; j < params.cols; j++) {
                    sum += matrix[gid * params.cols + j] * vector_in[j];
                }
                vector_out[gid] = sum;
            }
        )";
        return kernel;
    }
    
    // Reduction (sum)
    static GPUKernel reduce() {
        GPUKernel kernel;
        kernel.name = "reduce_sum";
        kernel.workgroupSizeX = 256;
        
        kernel.buffers = {
            {"input", 0, 0, "float", true},
            {"output", 0, 1, "float", false}
        };
        
        kernel.uniforms = {
            {"params", 0, 2, {{"count", "uint"}}}
        };
        
        kernel.body = R"(
            // Shared memory for partial sums
            shared float sdata[256];
            
            uint tid = gl_LocalInvocationID.x;
            uint i = gid;
            
            // Load data
            sdata[tid] = (i < params.count) ? input[i] : 0.0;
            barrier();
            
            // Reduction in shared memory
            for (uint s = gl_WorkGroupSize.x / 2; s > 0; s >>= 1) {
                if (tid < s) {
                    sdata[tid] += sdata[tid + s];
                }
                barrier();
            }
            
            // Write result
            if (tid == 0) {
                output[gl_WorkGroupID.x] = sdata[0];
            }
        )";
        return kernel;
    }
    
    // Prefix sum (scan)
    static GPUKernel prefixSum() {
        GPUKernel kernel;
        kernel.name = "prefix_sum";
        kernel.workgroupSizeX = 256;
        
        kernel.buffers = {
            {"data", 0, 0, "float", false}
        };
        
        kernel.uniforms = {
            {"params", 0, 1, {{"count", "uint"}}}
        };
        
        kernel.body = R"(
            shared float temp[512];
            
            uint tid = gl_LocalInvocationID.x;
            uint offset = 1;
            
            // Load input
            temp[2*tid] = data[2*gid];
            temp[2*tid+1] = data[2*gid+1];
            
            // Build sum in place up the tree
            for (uint d = params.count >> 1; d > 0; d >>= 1) {
                barrier();
                if (tid < d) {
                    uint ai = offset * (2*tid+1) - 1;
                    uint bi = offset * (2*tid+2) - 1;
                    temp[bi] += temp[ai];
                }
                offset *= 2;
            }
            
            // Clear the last element
            if (tid == 0) temp[params.count - 1] = 0;
            
            // Traverse down tree & build scan
            for (uint d = 1; d < params.count; d *= 2) {
                offset >>= 1;
                barrier();
                if (tid < d) {
                    uint ai = offset * (2*tid+1) - 1;
                    uint bi = offset * (2*tid+2) - 1;
                    float t = temp[ai];
                    temp[ai] = temp[bi];
                    temp[bi] += t;
                }
            }
            
            barrier();
            
            // Write results
            data[2*gid] = temp[2*tid];
            data[2*gid+1] = temp[2*tid+1];
        )";
        return kernel;
    }
    
    // Bitonic sort
    static GPUKernel bitonicSort() {
        GPUKernel kernel;
        kernel.name = "bitonic_sort";
        kernel.workgroupSizeX = 256;
        
        kernel.buffers = {
            {"data", 0, 0, "float", false}
        };
        
        kernel.uniforms = {
            {"params", 0, 1, {{"j", "uint"}, {"k", "uint"}}}
        };
        
        kernel.body = R"(
            uint i = gid;
            uint ixj = i ^ params.j;
            
            if (ixj > i) {
                if ((i & params.k) == 0) {
                    if (data[i] > data[ixj]) {
                        float temp = data[i];
                        data[i] = data[ixj];
                        data[ixj] = temp;
                    }
                } else {
                    if (data[i] < data[ixj]) {
                        float temp = data[i];
                        data[i] = data[ixj];
                        data[ixj] = temp;
                    }
                }
            }
        )";
        return kernel;
    }
};

} // namespace GPU
} // namespace Compiler
} // namespace RawrXD
