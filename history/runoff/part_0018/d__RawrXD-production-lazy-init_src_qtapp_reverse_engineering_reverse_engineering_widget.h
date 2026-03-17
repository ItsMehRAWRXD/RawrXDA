/**
 * \file reverse_engineering_widget.h
 * \brief Main reverse engineering UI widget
 * \author RawrXD Team
 * \date 2026-01-22
 */

#pragma once

#include <QWidget>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QProgressBar>
#include <QComboBox>
#include <QSplitter>
#include "binary_loader.h"
#include "symbol_analyzer.h"
#include "disassembler.h"
#include "decompiler.h"
#include "struct_reconstructor.h"
#include "hex_editor.h"
#include "resource_extractor.h"
#include "symbol_manager.h"
#include <memory>

namespace RawrXD {
namespace ReverseEngineering {

/**
 * \class ReverseEngineeringWidget
 * \brief Main UI for reverse engineering features
 */
class ReverseEngineeringWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * \brief Construct reverse engineering widget
     * \param parent Parent widget
     */
    explicit ReverseEngineeringWidget(QWidget* parent = nullptr);
    
    /**
     * \brief Destructor
     */
    ~ReverseEngineeringWidget() override;
    
    /**
     * \brief Open binary file for analysis
     * \param filePath Path to binary file
     * \return True if successful
     */
    bool openBinary(const QString& filePath);
    
    /**
     * \brief Get current binary path
     * \return Binary file path
     */
    QString currentBinaryPath() const { return m_currentBinaryPath; }

signals:
    /**
     * \brief Emitted when analysis starts
     */
    void analysisStarted(const QString& binaryPath);
    
    /**
     * \brief Emitted when analysis completes
     * \param success True if successful
     */
    void analysisCompleted(bool success);
    
    /**
     * \brief Emitted when progress updates
     * \param message Progress message
     * \param percentage Progress percentage (0-100)
     */
    void progressUpdated(const QString& message, int percentage);

private slots:
    void onBrowseBinary();
    void onAnalyzeNow();
    void onSymbolSelected(QTreeWidgetItem* item);
    void onFunctionSelected(QTreeWidgetItem* item);
    void onStringSelected(QTreeWidgetItem* item);
    void onRenameSymbol();
    void onAddComment();
    void onExportResults();
    void onTabChanged(int index);
    void onSearchSymbols(const QString& text);
    void onRefreshAnalysis();

private:
    void setupUI();
    void createBinaryInfoTab();
    void createSymbolsTab();
    void createDisassemblyTab();
    void createDecompilationTab();
    void createStructsTab();
    void createHexTab();
    void createStringsTab();
    void createReportsTab();
    
    void updateBinaryInfo();
    void populateSymbolsTree();
    void populateFunctionsTree();
    void populateStringsTree();
    void updateDisassembly(const QString& functionName);
    void updateDecompilation(const QString& functionName);
    void updateStructInfo(const QString& structName);
    void updateHexEditor(uint64_t address, size_t size);
    
    // UI Components
    QTabWidget* m_tabWidget;
    
    // Binary Info Tab
    QLabel* m_binaryPathLabel;
    QLabel* m_formatLabel;
    QLabel* m_architectureLabel;
    QLabel* m_sectionsLabel;
    QLabel* m_exportsLabel;
    QLabel* m_importsLabel;
    
    // Symbols Tab
    QLineEdit* m_symbolSearchEdit;
    QTreeWidget* m_symbolsTree;
    QTextEdit* m_symbolDetailsEdit;
    QPushButton* m_renameButton;
    QPushButton* m_commentButton;
    
    // Disassembly Tab
    QComboBox* m_functionCombo;
    QTextEdit* m_disassemblyEdit;
    QTreeWidget* m_xrefsTree;
    
    // Decompilation Tab
    QTextEdit* m_decompilationEdit;
    QLabel* m_complexityLabel;
    
    // Structs Tab
    QTreeWidget* m_structsTree;
    QTextEdit* m_structCodeEdit;
    QPushButton* m_editStructButton;
    
    // Hex Editor Tab
    QLineEdit* m_hexAddressEdit;
    QTextEdit* m_hexViewEdit;
    QPushButton* m_hexSearchButton;
    
    // Strings Tab
    QTreeWidget* m_stringsTree;
    QLineEdit* m_stringSearchEdit;
    
    // Reports Tab
    QTextEdit* m_reportEdit;
    QPushButton* m_generateReportButton;
    QPushButton* m_exportButton;
    
    // Analysis components
    QString m_currentBinaryPath;
    std::unique_ptr<BinaryLoader> m_loader;
    std::unique_ptr<SymbolAnalyzer> m_symbolAnalyzer;
    std::unique_ptr<Disassembler> m_disassembler;
    std::unique_ptr<Decompiler> m_decompiler;
    std::unique_ptr<StructReconstructor> m_structReconstructor;
    std::unique_ptr<HexEditor> m_hexEditor;
    std::unique_ptr<ResourceExtractor> m_resourceExtractor;
    std::unique_ptr<SymbolManager> m_symbolManager;
    
    // Progress tracking
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
    
    bool isAnalyzing;
};

} // namespace ReverseEngineering
} // namespace RawrXD
