/**
 * \file binary_loader.h
 * \brief Binary executable loader and parser for PE/ELF/Mach-O formats
 * \author RawrXD Team
 * \date 2026-01-22
 * 
 * Supports loading and parsing of:
 * - PE (Portable Executable) - Windows .exe/.dll
 * - ELF (Executable and Linkable Format) - Linux/Unix binaries
 * - Mach-O (Mach Object) - macOS binaries
 */

#pragma once

#include <QString>
#include <QByteArray>
#include <QVector>
#include <QMap>
#include <cstdint>
#include <memory>

namespace RawrXD {
namespace ReverseEngineering {

/**
 * \enum Architecture
 * \brief Supported CPU architectures
 */
enum class Architecture {
    UNKNOWN,
    X86,        // 32-bit x86
    X64,        // 64-bit x86-64
    ARM,        // 32-bit ARM
    ARM64,      // 64-bit ARM
    MIPS,       // MIPS architecture
    POWERPC     // PowerPC architecture
};

/**
 * \enum BinaryFormat
 * \brief Supported binary file formats
 */
enum class BinaryFormat {
    UNKNOWN,
    PE,         // Windows PE executable
    ELF,        // Unix/Linux ELF
    MACHO       // macOS Mach-O
};

/**
 * \struct SectionInfo
 * \brief Information about a binary section
 */
struct SectionInfo {
    QString name;               ///< Section name (e.g., ".text", ".data")
    uint64_t virtualAddress;    ///< Virtual address in memory
    uint64_t virtualSize;       ///< Size in memory
    uint64_t fileOffset;        ///< Offset in file
    uint64_t fileSize;          ///< Size in file
    uint32_t flags;             ///< Section flags/characteristics
    bool isExecutable;          ///< True if code section
    bool isWritable;            ///< True if writable
    bool isReadable;            ///< True if readable
};

/**
 * \struct ExportInfo
 * \brief Information about an exported function/symbol
 */
struct ExportInfo {
    QString name;               ///< Export name
    uint64_t address;           ///< RVA (relative virtual address)
    uint32_t ordinal;           ///< Ordinal number (PE only)
    bool isForwarded;           ///< True if forwarded export
    QString forwardedTo;        ///< Forwarded target (if forwarded)
};

/**
 * \struct ImportInfo
 * \brief Information about an imported function/symbol
 */
struct ImportInfo {
    QString name;               ///< Import name
    QString moduleName;         ///< DLL/module name
    uint64_t address;           ///< IAT address
    uint32_t hint;              ///< Hint (PE only)
    bool isOrdinal;             ///< True if imported by ordinal
};

/**
 * \struct BinaryMetadata
 * \brief Core metadata about a binary
 */
struct BinaryMetadata {
    BinaryFormat format;        ///< Binary format (PE/ELF/Mach-O)
    Architecture architecture;  ///< CPU architecture
    bool is64Bit;               ///< True if 64-bit binary
    bool isDebugBuild;          ///< True if debug symbols present
    uint64_t imageBase;         ///< Base address in virtual memory
    uint64_t entryPoint;        ///< Entry point RVA
    uint32_t sectionCount;      ///< Number of sections
    uint32_t exportCount;       ///< Number of exports
    uint32_t importCount;       ///< Number of imports
    QString subsystem;          ///< Subsystem (Windows only)
    QString timestamp;          ///< Compilation timestamp
};

/**
 * \class BinaryLoader
 * \brief Main class for loading and parsing binary executables
 * 
 * Usage:
 * \code
 * BinaryLoader loader("C:\\app.exe");
 * if (loader.load()) {
 *     auto metadata = loader.metadata();
 *     auto sections = loader.sections();
 *     auto exports = loader.exports();
 * }
 * \endcode
 */
class BinaryLoader {
public:
    /**
     * \brief Construct binary loader for file path
     * \param filePath Absolute path to binary file
     */
    explicit BinaryLoader(const QString& filePath);
    
    /**
     * \brief Destructor
     */
    ~BinaryLoader();
    
    /**
     * \brief Load and parse binary file
     * \return True if successful
     */
    bool load();
    
    /**
     * \brief Get loaded binary metadata
     * \return Binary metadata structure
     */
    const BinaryMetadata& metadata() const { return m_metadata; }
    
    /**
     * \brief Get all sections
     * \return Vector of section information
     */
    const QVector<SectionInfo>& sections() const { return m_sections; }
    
    /**
     * \brief Get all exported symbols
     * \return Vector of export information
     */
    const QVector<ExportInfo>& exports() const { return m_exports; }
    
    /**
     * \brief Get all imported symbols
     * \return Vector of import information
     */
    const QVector<ImportInfo>& imports() const { return m_imports; }
    
    /**
     * \brief Get raw file data
     * \return Raw binary data
     */
    const QByteArray& rawData() const { return m_fileData; }
    
    /**
     * \brief Get error message if load failed
     * \return Error description
     */
    QString errorMessage() const { return m_errorMessage; }
    
    /**
     * \brief Convert RVA to file offset
     * \param rva Relative virtual address
     * \return File offset, or 0 if not found
     */
    uint64_t rvaToFileOffset(uint64_t rva) const;
    
    /**
     * \brief Get bytes from binary at RVA
     * \param rva Relative virtual address
     * \param size Number of bytes to read
     * \return Byte array at location
     */
    QByteArray bytesAtRVA(uint64_t rva, size_t size) const;
    
    /**
     * \brief Find section by address
     * \param rva Relative virtual address
     * \return Pointer to section info, or nullptr if not found
     */
    const SectionInfo* sectionAtRVA(uint64_t rva) const;
    
    /**
     * \brief Detect binary format from file header
     * \param filePath Path to binary file
     * \return Detected binary format
     */
    static BinaryFormat detectFormat(const QString& filePath);
    
    /**
     * \brief Get human-readable architecture name
     * \param arch Architecture enum
     * \return Architecture name string
     */
    static QString architectureName(Architecture arch);
    
    /**
     * \brief Get human-readable format name
     * \param format Binary format enum
     * \return Format name string
     */
    static QString formatName(BinaryFormat format);

private:
    // PE-specific parsing
    bool loadPE();
    void parsePEHeaders();
    void parsePESections();
    void parsePEExports();
    void parsePEImports();
    
    // ELF-specific parsing
    bool loadELF();
    void parseELFHeaders();
    void parseELFSections();
    void parseELFSymbols();
    
    // Mach-O specific parsing
    bool loadMacho();
    void parseMachoHeaders();
    void parseMachoSections();
    void parseMachoSymbols();
    
    // Helper methods
    void setError(const QString& message);
    uint32_t readUint32LE(size_t offset) const;
    uint32_t readUint32BE(size_t offset) const;
    uint64_t readUint64LE(size_t offset) const;
    uint64_t readUint64BE(size_t offset) const;

    QString m_filePath;
    QByteArray m_fileData;
    BinaryMetadata m_metadata;
    QVector<SectionInfo> m_sections;
    QVector<ExportInfo> m_exports;
    QVector<ImportInfo> m_imports;
    QString m_errorMessage;
    bool m_isLittleEndian;
};

} // namespace ReverseEngineering
} // namespace RawrXD
