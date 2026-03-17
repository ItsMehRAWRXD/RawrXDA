/**
 * \file struct_reconstructor.cpp
 * \brief Implementation of struct reconstructor
 * \author RawrXD Team
 * \date 2026-01-22
 */

#include "struct_reconstructor.h"
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <algorithm>

using namespace RawrXD::ReverseEngineering;

StructReconstructor::StructReconstructor(const Decompiler& decompiler)
    : m_decompiler(decompiler) {
}

QVector<ReconstructedStruct> StructReconstructor::analyzeFunction(const DecompiledFunction& funcInfo) {
    QVector<ReconstructedStruct> foundStructs;
    
    // Analyze variables for struct patterns
    QMap<uint32_t, StructField> fields;
    analyzeMemoryAccesses(funcInfo, fields);
    
    if (!fields.isEmpty()) {
        // Find contiguous block of offsets
        QVector<uint32_t> offsets = fields.keys();
        std::sort(offsets.begin(), offsets.end());
        
        uint32_t maxOffset = offsets.last();
        uint32_t maxSize = 0;
        
        for (const auto& field : fields.values()) {
            maxSize = std::max(maxSize, field.offset + field.size);
        }
        
        if (maxSize > 0) {
            ReconstructedStruct reconstructed;
            reconstructed.name = QString("struct_%1").arg(funcInfo.address, 8, 16, QChar('0'));
            reconstructed.size = maxSize;
            reconstructed.address = funcInfo.address;
            
            for (auto it = fields.begin(); it != fields.end(); ++it) {
                reconstructed.fields.append(it.value());
            }
            
            addPaddingFields(reconstructed);
            reconstructed.confidence = calculateConfidence(reconstructed.fields);
            reconstructed.sourceCode = generateCode(reconstructed);
            
            m_structs.append(reconstructed);
            m_structsByName[reconstructed.name] = reconstructed;
            foundStructs.append(reconstructed);
        }
    }
    
    qDebug() << "Found" << foundStructs.size() << "structs in function" << funcInfo.name;
    return foundStructs;
}

void StructReconstructor::analyzeMemoryAccesses(const DecompiledFunction& func, 
                                                 QMap<uint32_t, StructField>& fields) {
    // Analyze variable accesses to reconstruct struct layout
    
    for (const auto& var : func.variables) {
        StructField field;
        field.name = var.name;
        field.offset = std::abs(var.stackOffset);
        field.type = inferFieldType(var, 8);
        field.size = 8;  // Default to 64-bit
        field.typeName = typeToString(field.type);
        
        fields[field.offset] = field;
    }
}

FieldType StructReconstructor::inferFieldType(const Variable& var, int accessSize) {
    // Infer type from access size and usage patterns
    
    switch (accessSize) {
        case 1:
            return FieldType::INT8;
        case 2:
            return FieldType::INT16;
        case 4:
            return FieldType::INT32;
        case 8:
            return FieldType::INT64;
        default:
            if (var.name.contains("ptr") || var.name.contains("handle")) {
                return FieldType::POINTER;
            }
            return FieldType::UNKNOWN;
    }
}

ReconstructedStruct StructReconstructor::createStruct(const QString& name, uint32_t size,
                                                      const QMap<uint32_t, QPair<QString, uint32_t>>& fieldOffsets) {
    ReconstructedStruct structInfo;
    structInfo.name = name;
    structInfo.size = size;
    structInfo.isClass = false;
    
    for (auto it = fieldOffsets.begin(); it != fieldOffsets.end(); ++it) {
        StructField field;
        field.offset = it.key();
        field.size = it.value().second;
        field.name = it.value().first;
        field.type = FieldType::UNKNOWN;
        field.typeName = "void*";
        
        structInfo.fields.append(field);
    }
    
    std::sort(structInfo.fields.begin(), structInfo.fields.end(),
              [](const StructField& a, const StructField& b) {
                  return a.offset < b.offset;
              });
    
    addPaddingFields(structInfo);
    structInfo.sourceCode = generateCode(structInfo);
    
    m_structs.append(structInfo);
    m_structsByName[name] = structInfo;
    
    return structInfo;
}

QString StructReconstructor::generateCode(const ReconstructedStruct& structInfo) {
    QString code;
    
    // Generate C++ struct definition
    code += structInfo.isClass ? "class " : "struct ";
    code += structInfo.name + " {\n";
    
    if (structInfo.isClass) {
        code += "public:\n";
    }
    
    // Generate fields
    for (const auto& field : structInfo.fields) {
        if (!field.isPadding) {
            code += QString("  %1 %2; // offset: 0x%3\n")
                    .arg(field.typeName)
                    .arg(field.name)
                    .arg(field.offset, 2, 16, QChar('0'));
        } else {
            code += QString("  char _padding_%1[0x%2]; // 0x%3-0x%4\n")
                    .arg(field.offset)
                    .arg(field.size, 2, 16, QChar('0'))
                    .arg(field.offset, 2, 16, QChar('0'))
                    .arg(field.offset + field.size, 2, 16, QChar('0'));
        }
    }
    
    code += "}; // size: 0x" + QString::number(structInfo.size, 16) + "\n";
    
    return code;
}

ReconstructedStruct StructReconstructor::findStruct(const QString& name) const {
    auto it = m_structsByName.find(name);
    if (it != m_structsByName.end()) {
        return it.value();
    }
    return ReconstructedStruct();
}

bool StructReconstructor::editStructField(const QString& structName, uint32_t fieldOffset,
                                          FieldType newType, const QString& newName) {
    // Find and edit struct field
    for (auto& structInfo : m_structs) {
        if (structInfo.name == structName) {
            for (auto& field : structInfo.fields) {
                if (field.offset == fieldOffset) {
                    field.type = newType;
                    field.name = newName;
                    field.typeName = typeToString(newType);
                    
                    // Regenerate code
                    structInfo.sourceCode = generateCode(structInfo);
                    m_structsByName[structName] = structInfo;
                    
                    return true;
                }
            }
        }
    }
    return false;
}

bool StructReconstructor::exportAsHeader(const QString& filename) {
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Cannot open file for writing:" << filename;
        return false;
    }
    
    QTextStream stream(&file);
    
    // Write header
    stream << "#pragma once\n\n";
    stream << "// Auto-generated struct definitions\n";
    stream << "// Generated by RawrXD Reverse Engineering Engine\n\n";
    
    // Write all structs
    for (const auto& structInfo : m_structs) {
        stream << structInfo.sourceCode << "\n\n";
    }
    
    file.close();
    
    qDebug() << "Exported" << m_structs.size() << "structs to" << filename;
    return true;
}

int StructReconstructor::calculateConfidence(const QVector<StructField>& fields) {
    // Calculate confidence based on field coverage and alignment
    int confidence = 50;  // Base confidence
    
    // Bonus for well-aligned fields
    int alignedFields = 0;
    for (const auto& field : fields) {
        if (field.offset % field.size == 0) {
            alignedFields++;
        }
    }
    
    if (!fields.isEmpty()) {
        confidence += (alignedFields * 50) / fields.size();
    }
    
    return std::min(100, confidence);
}

void StructReconstructor::addPaddingFields(ReconstructedStruct& structInfo) {
    // Add padding fields to align struct layout
    
    QVector<StructField> allFields = structInfo.fields;
    std::sort(allFields.begin(), allFields.end(),
              [](const StructField& a, const StructField& b) {
                  return a.offset < b.offset;
              });
    
    QVector<StructField> withPadding;
    uint32_t currentPos = 0;
    
    for (const auto& field : allFields) {
        // Add padding if there's a gap
        if (field.offset > currentPos) {
            StructField padding;
            padding.name = QString("_padding_%1").arg(currentPos);
            padding.offset = currentPos;
            padding.size = field.offset - currentPos;
            padding.type = FieldType::UNKNOWN;
            padding.typeName = "char";
            padding.isPadding = true;
            
            withPadding.append(padding);
        }
        
        withPadding.append(field);
        currentPos = field.offset + field.size;
    }
    
    // Add final padding if needed
    if (currentPos < structInfo.size) {
        StructField padding;
        padding.name = QString("_padding_%1").arg(currentPos);
        padding.offset = currentPos;
        padding.size = structInfo.size - currentPos;
        padding.type = FieldType::UNKNOWN;
        padding.typeName = "char";
        padding.isPadding = true;
        
        withPadding.append(padding);
    }
    
    structInfo.fields = withPadding;
}

QString StructReconstructor::typeToString(FieldType type) const {
    switch (type) {
        case FieldType::INT8: return "int8_t";
        case FieldType::INT16: return "int16_t";
        case FieldType::INT32: return "int32_t";
        case FieldType::INT64: return "int64_t";
        case FieldType::FLOAT: return "float";
        case FieldType::DOUBLE: return "double";
        case FieldType::POINTER: return "void*";
        case FieldType::STRUCT: return "struct";
        case FieldType::CLASS: return "class";
        case FieldType::ARRAY: return "array";
        case FieldType::UNION: return "union";
        default: return "unknown";
    }
}
