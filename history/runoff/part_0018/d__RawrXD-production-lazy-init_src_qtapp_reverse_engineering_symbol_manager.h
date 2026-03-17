/**
 * \file symbol_manager.h
 * \brief Symbol table management and annotation
 * \author RawrXD Team
 * \date 2026-01-22
 */

#pragma once

#include "symbol_analyzer.h"
#include <QString>
#include <QVector>
#include <QMap>

namespace RawrXD {
namespace ReverseEngineering {

/**
 * \struct SymbolAnnotation
 * \brief Symbol annotation data
 */
struct SymbolAnnotation {
    QString originalName;       ///< Original name
    QString customName;         ///< User-given name
    QString type;               ///< Type/signature
    QString comment;            ///< User comment
    bool isConfirmed;           ///< User confirmed this symbol
    int confidence;             ///< Analysis confidence (0-100)
};

/**
 * \class SymbolManager
 * \brief Manages symbols with annotations and editing
 */
class SymbolManager {
public:
    /**
     * \brief Construct manager from analyzer
     * \param analyzer Symbol analyzer
     */
    explicit SymbolManager(const SymbolAnalyzer& analyzer);
    
    /**
     * \brief Rename symbol
     * \param originalName Original symbol name
     * \param newName New name
     * \return True if successful
     */
    bool renameSymbol(const QString& originalName, const QString& newName);
    
    /**
     * \brief Get custom name for symbol
     * \param symbolName Symbol name
     * \return Custom name, or original if not renamed
     */
    QString getDisplayName(const QString& symbolName) const;
    
    /**
     * \brief Add comment to symbol
     * \param symbolName Symbol name
     * \param comment Comment text
     */
    void addComment(const QString& symbolName, const QString& comment);
    
    /**
     * \brief Get symbol comment
     * \param symbolName Symbol name
     * \return Comment text, or empty if none
     */
    QString getComment(const QString& symbolName) const;
    
    /**
     * \brief Match function signatures
     * \param signature Function signature to match
     * \return Vector of potentially matching symbols
     */
    QVector<SymbolInfo> matchSignature(const QString& signature) const;
    
    /**
     * \brief Auto-rename symbols based on patterns
     * \return Number of symbols renamed
     */
    int autoRenameSymbols();
    
    /**
     * \brief Get all annotations
     * \return Map of symbol name -> annotation
     */
    const QMap<QString, SymbolAnnotation>& annotations() const { return m_annotations; }
    
    /**
     * \brief Export symbol table to CSV
     * \param filename Output filename
     * \return True if successful
     */
    bool exportToCSV(const QString& filename) const;
    
    /**
     * \brief Import symbol table from CSV
     * \param filename Input filename
     * \return True if successful
     */
    bool importFromCSV(const QString& filename);
    
    /**
     * \brief Apply standard naming conventions
     * \return Number of symbols renamed
     */
    int applyNamingConventions();
    
    /**
     * \brief Mark symbol as confirmed
     * \param symbolName Symbol name
     */
    void confirmSymbol(const QString& symbolName);
    
    /**
     * \brief Get confirmed symbols
     * \return Vector of confirmed symbols
     */
    QVector<SymbolInfo> confirmedSymbols() const;

private:
    const SymbolAnalyzer& m_analyzer;
    QMap<QString, SymbolAnnotation> m_annotations;
    
    QString suggestName(const SymbolInfo& symbol);
    bool looksLikeConstructor(const QString& name);
    bool looksLikeDestructor(const QString& name);
    bool looksLikeGetter(const QString& name);
    bool looksLikeSetter(const QString& name);
};

} // namespace ReverseEngineering
} // namespace RawrXD
