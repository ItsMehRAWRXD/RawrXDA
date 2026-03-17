/**
 * \file disassembler.cpp
 * \brief Implementation of disassembler
 * \author RawrXD Team
 * \date 2026-01-22
 */

#include "disassembler.h"
#include <QDebug>

using namespace RawrXD::ReverseEngineering;

Disassembler::Disassembler(const BinaryLoader& loader)
    : m_loader(loader), m_handle(0) {
    initCapstone();
}

Disassembler::~Disassembler() {
    if (m_handle) {
        cs_close(&m_handle);
    }
}

bool Disassembler::initCapstone() {
    cs_arch arch = CS_ARCH_X86;
    cs_mode mode = CS_MODE_32;
    
    // Map architecture to Capstone
    switch (m_loader.metadata().architecture) {
        case Architecture::X86:
            arch = CS_ARCH_X86;
            mode = CS_MODE_32;
            break;
        case Architecture::X64:
            arch = CS_ARCH_X86;
            mode = CS_MODE_64;
            break;
        case Architecture::ARM:
            arch = CS_ARCH_ARM;
            mode = CS_MODE_ARM;
            break;
        case Architecture::ARM64:
            arch = CS_ARCH_ARM64;
            mode = CS_MODE_ARM;
            break;
        default:
            qWarning() << "Unsupported architecture for disassembly";
            return false;
    }
    
    if (cs_open(arch, mode, &m_handle) != CS_ERR_OK) {
        qWarning() << "Failed to initialize Capstone";
        m_handle = 0;
        return false;
    }
    
    // Enable detailed disassembly
    cs_option(m_handle, CS_OPT_DETAIL, CS_OPT_ON);
    
    qDebug() << "Capstone initialized for" << BinaryLoader::architectureName(m_loader.metadata().architecture);
    return true;
}

FunctionInfo Disassembler::disassemble(uint64_t rva, uint64_t size, const QString& funcName) {
    FunctionInfo func;
    func.name = funcName.isEmpty() ? QString("sub_%1").arg(rva, 8, 16, QChar('0')) : funcName;
    func.address = rva;
    func.size = size;
    func.isRecognized = !funcName.isEmpty();
    
    if (!m_handle) {
        qWarning() << "Capstone not initialized";
        return func;
    }
    
    QByteArray codeBytes = m_loader.bytesAtRVA(rva, size);
    if (codeBytes.isEmpty()) {
        qWarning() << "Failed to read code bytes at RVA" << rva;
        return func;
    }
    
    cs_insn* insn = nullptr;
    size_t insn_count = cs_disasm(m_handle, reinterpret_cast<const uint8_t*>(codeBytes.constData()),
                                   codeBytes.size(), rva, 0, &insn);
    
    if (insn_count == 0) {
        qWarning() << "Disassembly failed at RVA" << rva;
        return func;
    }
    
    for (size_t i = 0; i < insn_count; ++i) {
        Instruction instr;
        instr.address = insn[i].address;
        instr.bytes = QByteArray(reinterpret_cast<const char*>(insn[i].bytes), insn[i].size);
        instr.mnemonic = QString::fromUtf8(insn[i].mnemonic);
        instr.operands = QString::fromUtf8(insn[i].op_str);
        instr.disassembly = QString::fromUtf8(insn[i].mnemonic) + " " + QString::fromUtf8(insn[i].op_str);
        
        // Detect instruction types
        if (insn[i].id == X86_INS_CALL || insn[i].id == ARM_INS_BL || insn[i].id == ARM64_INS_BL) {
            instr.isCall = true;
        }
        if ((insn[i].id >= X86_INS_JA && insn[i].id <= X86_INS_JZ) ||
            insn[i].id == X86_INS_JMP ||
            (insn[i].id >= ARM_INS_BEQ && insn[i].id <= ARM_INS_BX)) {
            instr.isBranch = true;
        }
        if (insn[i].id == X86_INS_RET || insn[i].id == ARM_INS_POP || insn[i].id == ARM64_INS_RET) {
            instr.isReturn = true;
        }
        
        func.instructions.append(instr);
        m_allInstructions.append(instr);
    }
    
    cs_free(insn, insn_count);
    
    qDebug() << "Disassembled" << func.instructions.size() << "instructions at" << func.name;
    return func;
}

FunctionInfo Disassembler::functionAt(uint64_t address) const {
    for (const auto& func : m_functions) {
        if (address >= func.address && address < func.address + func.size) {
            return func;
        }
    }
    return FunctionInfo();
}

FunctionInfo Disassembler::functionNamed(const QString& name) const {
    auto it = m_functionsByName.find(name);
    if (it != m_functionsByName.end()) {
        return it.value();
    }
    return FunctionInfo();
}

Instruction Disassembler::instructionAt(uint64_t address) const {
    for (const auto& instr : m_allInstructions) {
        if (instr.address == address) {
            return instr;
        }
    }
    return Instruction();
}

int Disassembler::analyzeCodeSection(const QString& sectionName) {
    const SectionInfo* section = nullptr;
    
    for (const auto& sec : m_loader.sections()) {
        if (sec.name == sectionName || (sectionName == ".text" && sec.name.contains("text"))) {
            section = &sec;
            break;
        }
    }
    
    if (!section || !section->isExecutable) {
        qWarning() << "Code section not found:" << sectionName;
        return 0;
    }
    
    detectFunctionBoundaries(section->virtualAddress, section->virtualSize);
    
    return m_functions.size();
}

void Disassembler::detectFunctionBoundaries(uint64_t codeStart, uint64_t codeSize) {
    QByteArray codeBytes = m_loader.bytesAtRVA(codeStart, codeSize);
    if (codeBytes.isEmpty()) {
        return;
    }
    
    if (!m_handle) {
        return;
    }
    
    cs_insn* insn = nullptr;
    size_t insn_count = cs_disasm(m_handle, reinterpret_cast<const uint8_t*>(codeBytes.constData()),
                                   codeBytes.size(), codeStart, 0, &insn);
    
    if (insn_count == 0) {
        return;
    }
    
    uint64_t funcStart = codeStart;
    
    for (size_t i = 0; i < insn_count; ++i) {
        // Detect function boundaries
        if (i > 0 && isFunctionPrologue(Instruction{insn[i].address, {}, QString::fromUtf8(insn[i].mnemonic), {}, {}, false, false, false, 0})) {
            // Previous instruction might be end of function, start new function
            if (i > 0) {
                uint64_t funcSize = insn[i].address - funcStart;
                FunctionInfo func;
                func.address = funcStart;
                func.size = funcSize;
                func.name = QString("sub_%1").arg(funcStart, 8, 16, QChar('0'));
                
                FunctionInfo disasm = disassemble(funcStart, funcSize, func.name);
                m_functions.append(disasm);
                m_functionsByAddress[funcStart] = disasm;
                m_functionsByName[disasm.name] = disasm;
                
                funcStart = insn[i].address;
            }
        }
        
        if (isFunctionEpilogue(Instruction{insn[i].address, {}, QString::fromUtf8(insn[i].mnemonic), {}, {}, false, false, false, 0})) {
            // Function ends here
            uint64_t funcSize = (i + 1 < insn_count) ? (insn[i + 1].address - funcStart) : (insn[i].address + insn[i].size - funcStart);
            FunctionInfo func;
            func.address = funcStart;
            func.size = funcSize;
            func.name = QString("sub_%1").arg(funcStart, 8, 16, QChar('0'));
            
            FunctionInfo disasm = disassemble(funcStart, funcSize, func.name);
            m_functions.append(disasm);
            m_functionsByAddress[funcStart] = disasm;
            m_functionsByName[disasm.name] = disasm;
            
            funcStart = (i + 1 < insn_count) ? insn[i + 1].address : codeStart + codeSize;
        }
    }
    
    // Handle last function
    if (funcStart < codeStart + codeSize) {
        uint64_t funcSize = codeStart + codeSize - funcStart;
        FunctionInfo func;
        func.address = funcStart;
        func.size = funcSize;
        func.name = QString("sub_%1").arg(funcStart, 8, 16, QChar('0'));
        
        FunctionInfo disasm = disassemble(funcStart, funcSize, func.name);
        m_functions.append(disasm);
        m_functionsByAddress[funcStart] = disasm;
        m_functionsByName[disasm.name] = disasm;
    }
    
    cs_free(insn, insn_count);
    
    qDebug() << "Detected" << m_functions.size() << "functions in code section";
}

bool Disassembler::isFunctionPrologue(const Instruction& instr) {
    // Common function prologue patterns
    return instr.mnemonic == "push" || instr.mnemonic == "sub" || instr.mnemonic == "stp";
}

bool Disassembler::isFunctionEpilogue(const Instruction& instr) {
    // Common function epilogue patterns
    return instr.mnemonic == "ret" || instr.mnemonic == "retn" ||
           instr.mnemonic == "pop" || instr.mnemonic == "ldp";
}

uint64_t Disassembler::resolveJumpTarget(const Instruction& instr) {
    // Extract immediate from operands
    // This is a simplified implementation
    if (instr.operands.contains("0x")) {
        bool ok;
        uint64_t target = instr.operands.right(16).toULongLong(&ok, 16);
        if (ok) {
            return target;
        }
    }
    return 0;
}

QVector<Instruction> Disassembler::crossReferences(uint64_t target) const {
    QVector<Instruction> xrefs;
    
    for (const auto& instr : m_allInstructions) {
        if ((instr.isBranch || instr.isCall) && instr.target == target) {
            xrefs.append(instr);
        }
    }
    
    return xrefs;
}
