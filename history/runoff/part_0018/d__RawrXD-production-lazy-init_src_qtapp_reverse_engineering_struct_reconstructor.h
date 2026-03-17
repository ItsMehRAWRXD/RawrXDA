/**
 * \file struct_reconstructor.h
 * \brief Reconstruct C/C++ structs and classes from binary analysis
 * \author RawrXD Team
 * \date 2026-01-22
 */

#pragma once

#include "decompiler.h"
#include <QString>
#include <QVector>
#include <QMap>

namespace RawrXD {
namespace ReverseEngineering {

/**
 * \enum FieldType
 * \brief Basic field types
 */
enum class FieldType {
    UNKNOWN,
    INT8,
    INT16,
    INT32,
    INT64,
    FLOAT,
    DOUBLE,
    POINTER,
    STRUCT,
    CLASS,
    ARRAY,
    UNION
};

/**
 * \struct StructField
 * \brief A field in a struct or class
 */
struct StructField {
    QString name;               ///< Field name
    FieldType type;             ///< Field type
    uint32_t offset;            ///< Offset in struct (bytes)
    uint32_t size;              ///< Field size (bytes)
    QString typeName;           ///< Type name for complex types
    uint32_t arraySize;         ///< Array size (if array)
    QString comment;            ///< User comment
    bool isPadding;             ///< True if auto-generated padding
};

/**
 * \struct ReconstructedStruct
 * \brief A reconstructed C/C++ struct or class
 */
struct ReconstructedStruct {
    QString name;               ///< Struct/class name
    uint64_t address;           ///< Address where type instance is used
    uint32_t size;              ///< Total struct size (bytes)
    QVector<StructField> fields; ///< All fields
    QString baseClass;          ///< Base class (for classes)
    bool isClass;               ///< True if class, false if struct
    QString sourceCode;         ///< Generated C/C++ code
    int confidence;             ///< Reconstruction confidence (0-100)
};

/**
 * \class StructReconstructor
 * \brief Reconstructs structs and classes from binary analysis
 */
class StructReconstructor {
public:
    /**
     * \brief Construct reconstructor from decompiler
     * \param decompiler Decompiler instance
     */
    explicit StructReconstructor(const Decompiler& decompiler);
    
    /**
     * \brief Analyze function and extract struct usage
     * \param funcInfo Function to analyze
     * \return Vector of reconstructed structs found
     */
    QVector<ReconstructedStruct> analyzeFunction(const DecompiledFunction& funcInfo);
    
    /**
     * \brief Manually create struct from offsets
     * \param name Struct name
     * \param size Total size
     * \param fieldOffsets Map of offset -> type name
     * \return Reconstructed struct
     */
    ReconstructedStruct createStruct(const QString& name, uint32_t size,
                                     const QMap<uint32_t, QPair<QString, uint32_t>>& fieldOffsets);
    
    /**
     * \brief Generate C++ code for struct
     * \param structInfo Struct to generate code for
     * \return C++ struct definition
     */
    QString generateCode(const ReconstructedStruct& structInfo);
    
    /**
     * \brief Get all reconstructed structs
     * \return Vector of all structs
     */
    const QVector<ReconstructedStruct>& structs() const { return m_structs; }
    
    /**
     * \brief Find struct by name
     * \param name Struct name
     * \return Reconstructed struct, or empty if not found
     */
    ReconstructedStruct findStruct(const QString& name) const;
    
    /**
     * \brief Edit struct field
     * \param structName Struct name
     * \param fieldOffset Field offset
     * \param newType New field type
     * \param newName New field name
     * \return True if successful
     */
    bool editStructField(const QString& structName, uint32_t fieldOffset,
                        FieldType newType, const QString& newName);
    
    /**
     * \brief Export all structs as C++ header
     * \param filename Output filename
     * \return True if successful
     */
    bool exportAsHeader(const QString& filename);

private:
    const Decompiler& m_decompiler;
    QVector<ReconstructedStruct> m_structs;
    QMap<QString, ReconstructedStruct> m_structsByName;
    
    // Analysis methods
    void analyzeMemoryAccesses(const DecompiledFunction& func, 
                              QMap<uint32_t, StructField>& fields);
    FieldType inferFieldType(const Variable& var, int accessSize);
    int calculateConfidence(const QVector<StructField>& fields);
    void addPaddingFields(ReconstructedStruct& structInfo);
    QString typeToString(FieldType type) const;
};

} // namespace ReverseEngineering
} // namespace RawrXD
