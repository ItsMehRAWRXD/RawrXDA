/**
 * \file binary_loader.cpp
 * \brief Implementation of binary loader and parser
 * \author RawrXD Team
 * \date 2026-01-22
 */

#include "binary_loader.h"
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <cstring>
#include <algorithm>

using namespace RawrXD::ReverseEngineering;

// PE Format constants
#define PE_MAGIC 0x4550  // "PE" signature
#define DOS_HEADER_SIZE 64
#define PE_SIGNATURE_OFFSET 0x3C

// ELF Format constants
#define ELF_MAGIC 0x464C457F  // "\x7FELF"
#define ELF_HEADER_SIZE_32 52
#define ELF_HEADER_SIZE_64 64

// Mach-O Format constants
#define MACHO_MAGIC_32 0xFEEDFACE
#define MACHO_MAGIC_64 0xFEEDFACF
#define MACHO_MAGIC_FAT 0xCAFEBABE

BinaryLoader::BinaryLoader(const QString& filePath)
    : m_filePath(filePath), m_isLittleEndian(true) {
    m_metadata = {};
    m_metadata.format = BinaryFormat::UNKNOWN;
    m_metadata.architecture = Architecture::UNKNOWN;
}

BinaryLoader::~BinaryLoader() = default;

bool BinaryLoader::load() {
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(QString("Cannot open file: %1").arg(m_filePath));
        return false;
    }

    m_fileData = file.readAll();
    file.close();

    if (m_fileData.isEmpty()) {
        setError("Binary file is empty");
        return false;
    }

    // Detect format
    m_metadata.format = detectFormat(m_filePath);

    bool success = false;
    switch (m_metadata.format) {
        case BinaryFormat::PE:
            success = loadPE();
            break;
        case BinaryFormat::ELF:
            success = loadELF();
            break;
        case BinaryFormat::MACHO:
            success = loadMacho();
            break;
        default:
            setError("Unknown binary format");
            return false;
    }

    return success;
}

BinaryFormat BinaryLoader::detectFormat(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return BinaryFormat::UNKNOWN;
    }

    QByteArray header = file.read(4);
    file.close();

    if (header.size() < 4) {
        return BinaryFormat::UNKNOWN;
    }

    uint32_t magic = *reinterpret_cast<uint32_t*>(header.data());

    // Check for ELF
    if (magic == ELF_MAGIC) {
        return BinaryFormat::ELF;
    }

    // Check for PE
    if (header[0] == 'M' && header[1] == 'Z') {
        return BinaryFormat::PE;
    }

    // Check for Mach-O
    if (magic == MACHO_MAGIC_32 || magic == MACHO_MAGIC_64 || magic == MACHO_MAGIC_FAT) {
        return BinaryFormat::MACHO;
    }

    return BinaryFormat::UNKNOWN;
}

bool BinaryLoader::loadPE() {
    if (m_fileData.size() < DOS_HEADER_SIZE) {
        setError("PE file too small for DOS header");
        return false;
    }

    // Read PE offset from DOS header
    uint32_t peOffset = readUint32LE(PE_SIGNATURE_OFFSET);
    if (peOffset + 4 > static_cast<uint32_t>(m_fileData.size())) {
        setError("Invalid PE offset");
        return false;
    }

    // Verify PE signature
    uint32_t peSignature = readUint32LE(peOffset);
    if ((peSignature & 0xFFFF) != PE_MAGIC) {
        setError("Invalid PE signature");
        return false;
    }

    parsePEHeaders();
    parsePESections();
    parsePEExports();
    parsePEImports();

    return true;
}

void BinaryLoader::parsePEHeaders() {
    uint32_t peOffset = readUint32LE(PE_SIGNATURE_OFFSET);
    
    // PE file header
    uint16_t machine = readUint32LE(peOffset + 4) & 0xFFFF;
    uint16_t sectionCount = readUint32LE(peOffset + 6) & 0xFFFF;
    uint32_t characteristics = readUint32LE(peOffset + 22) & 0xFFFF;

    // Detect architecture
    switch (machine) {
        case 0x014C: m_metadata.architecture = Architecture::X86; m_metadata.is64Bit = false; break;
        case 0x8664: m_metadata.architecture = Architecture::X64; m_metadata.is64Bit = true; break;
        case 0x01C0: m_metadata.architecture = Architecture::ARM; m_metadata.is64Bit = false; break;
        case 0xAA64: m_metadata.architecture = Architecture::ARM64; m_metadata.is64Bit = true; break;
        default: m_metadata.architecture = Architecture::UNKNOWN; break;
    }

    m_metadata.sectionCount = sectionCount;
    m_metadata.isDebugBuild = (characteristics & 0x0010) != 0;

    // Read optional header to get image base and entry point
    uint16_t optionalHeaderSize = readUint32LE(peOffset + 20) & 0xFFFF;
    if (optionalHeaderSize > 0) {
        uint32_t optHeaderOffset = peOffset + 24;
        
        if (m_metadata.is64Bit) {
            m_metadata.imageBase = readUint64LE(optHeaderOffset + 24);
            m_metadata.entryPoint = readUint32LE(optHeaderOffset + 16);
        } else {
            m_metadata.imageBase = readUint32LE(optHeaderOffset + 28);
            m_metadata.entryPoint = readUint32LE(optHeaderOffset + 16);
        }
    }

    qDebug() << "PE Binary:" << architectureName(m_metadata.architecture)
             << (m_metadata.is64Bit ? "64-bit" : "32-bit");
}

void BinaryLoader::parsePESections() {
    uint32_t peOffset = readUint32LE(PE_SIGNATURE_OFFSET);
    uint16_t sectionCount = readUint32LE(peOffset + 6) & 0xFFFF;
    uint16_t optionalHeaderSize = readUint32LE(peOffset + 20) & 0xFFFF;

    uint32_t sectionTableOffset = peOffset + 24 + optionalHeaderSize;

    for (uint32_t i = 0; i < sectionCount; ++i) {
        uint32_t offset = sectionTableOffset + (i * 40);
        if (offset + 40 > static_cast<uint32_t>(m_fileData.size())) {
            break;
        }

        SectionInfo section;
        section.name = QString::fromLatin1(m_fileData.constData() + offset, 8).trimmed();
        section.virtualSize = readUint32LE(offset + 8);
        section.virtualAddress = readUint32LE(offset + 12);
        section.fileSize = readUint32LE(offset + 16);
        section.fileOffset = readUint32LE(offset + 20);
        section.flags = readUint32LE(offset + 36);

        // Parse flags
        section.isExecutable = (section.flags & 0x20000000) != 0;
        section.isWritable = (section.flags & 0x80000000) != 0;
        section.isReadable = (section.flags & 0x40000000) != 0;

        m_sections.append(section);
    }

    qDebug() << "Loaded" << m_sections.size() << "PE sections";
}

void BinaryLoader::parsePEExports() {
    // Get export table RVA from data directories
    uint32_t peOffset = readUint32LE(PE_SIGNATURE_OFFSET);
    uint16_t optionalHeaderSize = readUint32LE(peOffset + 20) & 0xFFFF;
    uint32_t optHeaderOffset = peOffset + 24;

    uint32_t exportTableRVA = 0;
    uint32_t exportTableSize = 0;

    if (m_metadata.is64Bit && optionalHeaderSize >= 112) {
        exportTableRVA = readUint32LE(optHeaderOffset + 96);
        exportTableSize = readUint32LE(optHeaderOffset + 100);
    } else if (!m_metadata.is64Bit && optionalHeaderSize >= 96) {
        exportTableRVA = readUint32LE(optHeaderOffset + 96);
        exportTableSize = readUint32LE(optHeaderOffset + 100);
    }

    if (exportTableRVA == 0 || exportTableSize == 0) {
        return;  // No exports
    }

    uint64_t exportOffset = rvaToFileOffset(exportTableRVA);
    if (exportOffset == 0) {
        return;
    }

    // Parse export directory
    uint32_t numberOfNames = readUint32LE(exportOffset + 24);
    uint32_t addressOfNames = readUint32LE(exportOffset + 32);
    uint32_t addressOfFunctions = readUint32LE(exportOffset + 28);

    uint64_t namesOffset = rvaToFileOffset(addressOfNames);
    uint64_t functionsOffset = rvaToFileOffset(addressOfFunctions);

    if (namesOffset == 0 || functionsOffset == 0) {
        return;
    }

    for (uint32_t i = 0; i < numberOfNames && i < 10000; ++i) {  // Limit to 10k exports
        uint32_t nameRVA = readUint32LE(namesOffset + (i * 4));
        uint64_t nameOffset = rvaToFileOffset(nameRVA);
        
        if (nameOffset > 0 && nameOffset < static_cast<uint64_t>(m_fileData.size())) {
            ExportInfo exp;
            exp.name = QString::fromLatin1(m_fileData.constData() + nameOffset);
            exp.address = nameRVA;
            m_exports.append(exp);
        }
    }

    m_metadata.exportCount = m_exports.size();
    qDebug() << "Loaded" << m_exports.size() << "PE exports";
}

void BinaryLoader::parsePEImports() {
    // Get import table RVA from data directories
    uint32_t peOffset = readUint32LE(PE_SIGNATURE_OFFSET);
    uint16_t optionalHeaderSize = readUint32LE(peOffset + 20) & 0xFFFF;
    uint32_t optHeaderOffset = peOffset + 24;

    uint32_t importTableRVA = 0;
    uint32_t importTableSize = 0;

    if (m_metadata.is64Bit && optionalHeaderSize >= 112) {
        importTableRVA = readUint32LE(optHeaderOffset + 8);
        importTableSize = readUint32LE(optHeaderOffset + 12);
    } else if (!m_metadata.is64Bit && optionalHeaderSize >= 96) {
        importTableRVA = readUint32LE(optHeaderOffset + 8);
        importTableSize = readUint32LE(optHeaderOffset + 12);
    }

    if (importTableRVA == 0 || importTableSize == 0) {
        return;  // No imports
    }

    uint64_t importOffset = rvaToFileOffset(importTableRVA);
    if (importOffset == 0) {
        return;
    }

    // Parse import descriptors (each is 20 bytes)
    for (uint32_t i = 0; i * 20 < importTableSize; ++i) {
        uint32_t descriptorOffset = importOffset + (i * 20);
        uint32_t nameRVA = readUint32LE(descriptorOffset + 12);

        if (nameRVA == 0) {
            break;  // End of descriptors
        }

        uint64_t nameOffset = rvaToFileOffset(nameRVA);
        if (nameOffset == 0 || nameOffset >= static_cast<uint64_t>(m_fileData.size())) {
            continue;
        }

        QString moduleName = QString::fromLatin1(m_fileData.constData() + nameOffset);

        uint32_t iatRVA = readUint32LE(descriptorOffset + 16);
        uint64_t iatOffset = rvaToFileOffset(iatRVA);

        if (iatOffset == 0) {
            continue;
        }

        // Parse import address table entries
        for (uint32_t j = 0; j < 10000; ++j) {  // Limit entries
            uint64_t entry;
            if (m_metadata.is64Bit) {
                entry = readUint64LE(iatOffset + (j * 8));
                if (entry == 0) break;
            } else {
                entry = readUint32LE(iatOffset + (j * 4));
                if (entry == 0) break;
            }

            ImportInfo imp;
            imp.moduleName = moduleName;
            imp.address = iatRVA + (j * (m_metadata.is64Bit ? 8 : 4));
            m_imports.append(imp);
        }
    }

    m_metadata.importCount = m_imports.size();
    qDebug() << "Loaded" << m_imports.size() << "PE imports";
}

bool BinaryLoader::loadELF() {
    if (m_fileData.size() < 20) {
        setError("ELF file too small");
        return false;
    }

    // Check ELF magic
    if (readUint32LE(0) != ELF_MAGIC) {
        setError("Invalid ELF magic");
        return false;
    }

    // Check if 32-bit or 64-bit
    uint8_t elfClass = m_fileData[4];
    m_metadata.is64Bit = (elfClass == 2);

    // Check endianness
    uint8_t elfData = m_fileData[5];
    m_isLittleEndian = (elfData == 1);

    parseELFHeaders();
    parseELFSections();
    parseELFSymbols();

    return true;
}

void BinaryLoader::parseELFHeaders() {
    // Parse e_machine field
    uint16_t machine = m_isLittleEndian ? readUint32LE(18) & 0xFFFF : readUint32BE(18) & 0xFFFF;

    switch (machine) {
        case 3: m_metadata.architecture = Architecture::X86; break;
        case 62: m_metadata.architecture = Architecture::X64; break;
        case 40: m_metadata.architecture = Architecture::ARM; break;
        case 183: m_metadata.architecture = Architecture::ARM64; break;
        default: m_metadata.architecture = Architecture::UNKNOWN; break;
    }

    qDebug() << "ELF Binary:" << architectureName(m_metadata.architecture)
             << (m_metadata.is64Bit ? "64-bit" : "32-bit");
}

void BinaryLoader::parseELFSections() {
    uint32_t sectionHeaderOffset;
    uint16_t sectionHeaderCount;
    uint16_t sectionHeaderSize;

    if (m_metadata.is64Bit) {
        if (m_fileData.size() < 64) return;
        sectionHeaderOffset = m_isLittleEndian ? readUint64LE(32) : readUint64BE(32);
        sectionHeaderCount = m_isLittleEndian ? readUint32LE(48) & 0xFFFF : readUint32BE(48) & 0xFFFF;
        sectionHeaderSize = m_isLittleEndian ? readUint32LE(58) & 0xFFFF : readUint32BE(58) & 0xFFFF;
    } else {
        if (m_fileData.size() < 52) return;
        sectionHeaderOffset = m_isLittleEndian ? readUint32LE(32) : readUint32BE(32);
        sectionHeaderCount = m_isLittleEndian ? readUint32LE(48) & 0xFFFF : readUint32BE(48) & 0xFFFF;
        sectionHeaderSize = m_isLittleEndian ? readUint32LE(46) & 0xFFFF : readUint32BE(46) & 0xFFFF;
    }

    for (uint16_t i = 0; i < sectionHeaderCount && i < 100; ++i) {
        uint32_t offset = sectionHeaderOffset + (i * sectionHeaderSize);
        if (offset + 32 > static_cast<uint32_t>(m_fileData.size())) {
            break;
        }

        SectionInfo section;
        uint32_t nameOffset = m_isLittleEndian ? readUint32LE(offset) : readUint32BE(offset);
        section.name = QString("Section_%1").arg(i);

        if (m_metadata.is64Bit) {
            section.virtualAddress = m_isLittleEndian ? readUint64LE(offset + 16) : readUint64BE(offset + 16);
            section.fileOffset = m_isLittleEndian ? readUint64LE(offset + 24) : readUint64BE(offset + 24);
            section.virtualSize = m_isLittleEndian ? readUint64LE(offset + 32) : readUint64BE(offset + 32);
            section.fileSize = m_isLittleEndian ? readUint64LE(offset + 40) : readUint64BE(offset + 40);
            section.flags = m_isLittleEndian ? readUint32LE(offset + 8) : readUint32BE(offset + 8);
        } else {
            section.virtualAddress = m_isLittleEndian ? readUint32LE(offset + 12) : readUint32BE(offset + 12);
            section.fileOffset = m_isLittleEndian ? readUint32LE(offset + 16) : readUint32BE(offset + 16);
            section.virtualSize = m_isLittleEndian ? readUint32LE(offset + 20) : readUint32BE(offset + 20);
            section.fileSize = m_isLittleEndian ? readUint32LE(offset + 24) : readUint32BE(offset + 24);
            section.flags = m_isLittleEndian ? readUint32LE(offset + 8) : readUint32BE(offset + 8);
        }

        section.isExecutable = (section.flags & 0x4) != 0;
        section.isWritable = (section.flags & 0x1) != 0;
        section.isReadable = (section.flags & 0x2) != 0;

        m_sections.append(section);
    }

    qDebug() << "Loaded" << m_sections.size() << "ELF sections";
}

void BinaryLoader::parseELFSymbols() {
    // Find symbol table
    for (const auto& section : m_sections) {
        if (section.name.contains("symtab") || section.name.contains("dynsym")) {
            // Symbol table found, would parse here
            // Limited implementation for now
        }
    }
}

bool BinaryLoader::loadMacho() {
    if (m_fileData.size() < 32) {
        setError("Mach-O file too small");
        return false;
    }

    uint32_t magic = readUint32LE(0);
    m_isLittleEndian = true;

    if (magic == MACHO_MAGIC_64 || magic == MACHO_MAGIC_32) {
        m_metadata.is64Bit = (magic == MACHO_MAGIC_64);
    } else if (magic == MACHO_MAGIC_FAT) {
        setError("Fat/Universal binaries not yet supported");
        return false;
    } else {
        setError("Invalid Mach-O magic");
        return false;
    }

    parseMachoHeaders();
    parseMachoSections();
    parseMachoSymbols();

    return true;
}

void BinaryLoader::parseMachoHeaders() {
    uint32_t cpuType = readUint32LE(4);

    switch (cpuType) {
        case 7: m_metadata.architecture = Architecture::X86; break;
        case 0x07000103: m_metadata.architecture = Architecture::X64; break;
        case 12: m_metadata.architecture = Architecture::ARM; break;
        case 0x0c000001: m_metadata.architecture = Architecture::ARM64; break;
        default: m_metadata.architecture = Architecture::UNKNOWN; break;
    }

    qDebug() << "Mach-O Binary:" << architectureName(m_metadata.architecture)
             << (m_metadata.is64Bit ? "64-bit" : "32-bit");
}

void BinaryLoader::parseMachoSections() {
    uint32_t ncmds = readUint32LE(m_metadata.is64Bit ? 16 : 12);
    uint32_t offset = m_metadata.is64Bit ? 32 : 28;

    for (uint32_t i = 0; i < ncmds && i < 100; ++i) {
        uint32_t cmd = readUint32LE(offset);
        uint32_t cmdSize = readUint32LE(offset + 4);

        if (cmd == 0x19) {  // LC_SEGMENT_64
            uint32_t nsects = readUint32LE(offset + 16);
            uint32_t sectOffset = offset + 48;

            for (uint32_t j = 0; j < nsects; ++j) {
                SectionInfo section;
                section.name = QString::fromLatin1(m_fileData.constData() + sectOffset, 16).trimmed();
                section.virtualAddress = readUint64LE(sectOffset + 24);
                section.virtualSize = readUint64LE(sectOffset + 32);
                section.fileOffset = readUint64LE(sectOffset + 40);
                section.fileSize = readUint64LE(sectOffset + 48);

                m_sections.append(section);
                sectOffset += 80;
            }
        }

        offset += cmdSize;
    }

    qDebug() << "Loaded" << m_sections.size() << "Mach-O sections";
}

void BinaryLoader::parseMachoSymbols() {
    // Limited implementation for now
}

uint64_t BinaryLoader::rvaToFileOffset(uint64_t rva) const {
    for (const auto& section : m_sections) {
        if (rva >= section.virtualAddress && 
            rva < section.virtualAddress + section.virtualSize) {
            return section.fileOffset + (rva - section.virtualAddress);
        }
    }
    return 0;
}

QByteArray BinaryLoader::bytesAtRVA(uint64_t rva, size_t size) const {
    uint64_t offset = rvaToFileOffset(rva);
    if (offset == 0 || offset + size > static_cast<uint64_t>(m_fileData.size())) {
        return QByteArray();
    }
    return m_fileData.mid(offset, size);
}

const SectionInfo* BinaryLoader::sectionAtRVA(uint64_t rva) const {
    for (const auto& section : m_sections) {
        if (rva >= section.virtualAddress && 
            rva < section.virtualAddress + section.virtualSize) {
            return &section;
        }
    }
    return nullptr;
}

QString BinaryLoader::architectureName(Architecture arch) {
    switch (arch) {
        case Architecture::X86: return "x86 (32-bit)";
        case Architecture::X64: return "x86-64 (64-bit)";
        case Architecture::ARM: return "ARM (32-bit)";
        case Architecture::ARM64: return "ARM64 (64-bit)";
        case Architecture::MIPS: return "MIPS";
        case Architecture::POWERPC: return "PowerPC";
        default: return "Unknown";
    }
}

QString BinaryLoader::formatName(BinaryFormat format) {
    switch (format) {
        case BinaryFormat::PE: return "PE (Windows)";
        case BinaryFormat::ELF: return "ELF (Unix/Linux)";
        case BinaryFormat::MACHO: return "Mach-O (macOS)";
        default: return "Unknown";
    }
}

void BinaryLoader::setError(const QString& message) {
    m_errorMessage = message;
    qWarning() << "BinaryLoader Error:" << message;
}

uint32_t BinaryLoader::readUint32LE(size_t offset) const {
    if (offset + 4 > static_cast<size_t>(m_fileData.size())) {
        return 0;
    }
    return *reinterpret_cast<uint32_t*>(const_cast<char*>(m_fileData.constData() + offset));
}

uint32_t BinaryLoader::readUint32BE(size_t offset) const {
    if (offset + 4 > static_cast<size_t>(m_fileData.size())) {
        return 0;
    }
    const uint8_t* data = reinterpret_cast<const uint8_t*>(m_fileData.constData() + offset);
    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

uint64_t BinaryLoader::readUint64LE(size_t offset) const {
    if (offset + 8 > static_cast<size_t>(m_fileData.size())) {
        return 0;
    }
    return *reinterpret_cast<uint64_t*>(const_cast<char*>(m_fileData.constData() + offset));
}

uint64_t BinaryLoader::readUint64BE(size_t offset) const {
    if (offset + 8 > static_cast<size_t>(m_fileData.size())) {
        return 0;
    }
    const uint8_t* data = reinterpret_cast<const uint8_t*>(m_fileData.constData() + offset);
    return (static_cast<uint64_t>(data[0]) << 56) | (static_cast<uint64_t>(data[1]) << 48) |
           (static_cast<uint64_t>(data[2]) << 40) | (static_cast<uint64_t>(data[3]) << 32) |
           (static_cast<uint64_t>(data[4]) << 24) | (static_cast<uint64_t>(data[5]) << 16) |
           (static_cast<uint64_t>(data[6]) << 8) | data[7];
}
