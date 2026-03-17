/**
 * \file reverse_engineering_widget.cpp
 * \brief Implementation of reverse engineering widget
 * \author RawrXD Team
 * \date 2026-01-22
 */

#include "reverse_engineering_widget.h"
#include "disassembler.h"
#include "decompiler.h"
#include "report_generator.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QDebug>
#include <QHeaderView>
#include <QProgressDialog>
#include <QTimer>

using namespace RawrXD::ReverseEngineering;

ReverseEngineeringWidget::ReverseEngineeringWidget(QWidget* parent)
    : QWidget(parent), isAnalyzing(false) {
    setupUI();
    qDebug() << "Reverse Engineering Widget initialized";
}

ReverseEngineeringWidget::~ReverseEngineeringWidget() = default;

void ReverseEngineeringWidget::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Binary file selector
    QHBoxLayout* fileLayout = new QHBoxLayout();
    m_binaryPathLabel = new QLabel("No binary loaded");
    QPushButton* browseButton = new QPushButton("Browse Binary");
    connect(browseButton, &QPushButton::clicked, this, &ReverseEngineeringWidget::onBrowseBinary);
    
    fileLayout->addWidget(new QLabel("Binary:"));
    fileLayout->addWidget(m_binaryPathLabel, 1);
    fileLayout->addWidget(browseButton);
    
    mainLayout->addLayout(fileLayout);
    
    // Progress bar
    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    mainLayout->addWidget(m_progressBar);
    
    m_statusLabel = new QLabel("Ready");
    mainLayout->addWidget(m_statusLabel);
    
    // Tab widget
    m_tabWidget = new QTabWidget();
    
    createBinaryInfoTab();
    createSymbolsTab();
    createDisassemblyTab();
    createDecompilationTab();
    createStructsTab();
    createHexTab();
    createStringsTab();
    createReportsTab();
    
    connect(m_tabWidget, QOverload<int>::of(&QTabWidget::currentChanged),
            this, &ReverseEngineeringWidget::onTabChanged);
    
    mainLayout->addWidget(m_tabWidget);
    
    setLayout(mainLayout);
}

void ReverseEngineeringWidget::createBinaryInfoTab() {
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout();
    
    m_binaryPathLabel = new QLabel("Path: N/A");
    m_formatLabel = new QLabel("Format: N/A");
    m_architectureLabel = new QLabel("Architecture: N/A");
    m_sectionsLabel = new QLabel("Sections: N/A");
    m_exportsLabel = new QLabel("Exports: N/A");
    m_importsLabel = new QLabel("Imports: N/A");
    
    layout->addWidget(m_binaryPathLabel);
    layout->addWidget(m_formatLabel);
    layout->addWidget(m_architectureLabel);
    layout->addWidget(m_sectionsLabel);
    layout->addWidget(m_exportsLabel);
    layout->addWidget(m_importsLabel);
    layout->addStretch();
    
    tab->setLayout(layout);
    m_tabWidget->addTab(tab, "Binary Info");
}

void ReverseEngineeringWidget::createSymbolsTab() {
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout();
    
    // Search bar
    QHBoxLayout* searchLayout = new QHBoxLayout();
    searchLayout->addWidget(new QLabel("Search:"));
    m_symbolSearchEdit = new QLineEdit();
    connect(m_symbolSearchEdit, &QLineEdit::textChanged, 
            this, &ReverseEngineeringWidget::onSearchSymbols);
    searchLayout->addWidget(m_symbolSearchEdit);
    layout->addLayout(searchLayout);
    
    // Symbols tree
    m_symbolsTree = new QTreeWidget();
    m_symbolsTree->setColumnCount(4);
    m_symbolsTree->setHeaderLabels({"Address", "Name", "Type", "Module"});
    m_symbolsTree->header()->setStretchLastSection(true);
    connect(m_symbolsTree, &QTreeWidget::itemSelected,
            this, &ReverseEngineeringWidget::onSymbolSelected);
    layout->addWidget(m_symbolsTree);
    
    // Details
    m_symbolDetailsEdit = new QTextEdit();
    m_symbolDetailsEdit->setReadOnly(true);
    m_symbolDetailsEdit->setMaximumHeight(100);
    layout->addWidget(new QLabel("Details:"));
    layout->addWidget(m_symbolDetailsEdit);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_renameButton = new QPushButton("Rename");
    connect(m_renameButton, &QPushButton::clicked, this, &ReverseEngineeringWidget::onRenameSymbol);
    m_commentButton = new QPushButton("Add Comment");
    connect(m_commentButton, &QPushButton::clicked, this, &ReverseEngineeringWidget::onAddComment);
    buttonLayout->addWidget(m_renameButton);
    buttonLayout->addWidget(m_commentButton);
    buttonLayout->addStretch();
    layout->addLayout(buttonLayout);
    
    tab->setLayout(layout);
    m_tabWidget->addTab(tab, "Symbols");
}

void ReverseEngineeringWidget::createDisassemblyTab() {
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout();
    
    // Function selector
    QHBoxLayout* funcLayout = new QHBoxLayout();
    funcLayout->addWidget(new QLabel("Function:"));
    m_functionCombo = new QComboBox();
    connect(m_functionCombo, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
            this, [this](const QString& text) { updateDisassembly(text); });
    funcLayout->addWidget(m_functionCombo);
    funcLayout->addStretch();
    layout->addLayout(funcLayout);
    
    // Disassembly view
    QSplitter* splitter = new QSplitter(Qt::Horizontal);
    
    m_disassemblyEdit = new QTextEdit();
    m_disassemblyEdit->setReadOnly(true);
    m_disassemblyEdit->setFont(QFont("Courier", 10));
    splitter->addWidget(m_disassemblyEdit);
    
    // Cross-references
    QWidget* xrefWidget = new QWidget();
    QVBoxLayout* xrefLayout = new QVBoxLayout(xrefWidget);
    xrefLayout->addWidget(new QLabel("Cross References:"));
    m_xrefsTree = new QTreeWidget();
    m_xrefsTree->setColumnCount(2);
    m_xrefsTree->setHeaderLabels({"Address", "Instruction"});
    xrefLayout->addWidget(m_xrefsTree);
    splitter->addWidget(xrefWidget);
    
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 1);
    
    layout->addWidget(splitter);
    
    tab->setLayout(layout);
    m_tabWidget->addTab(tab, "Disassembly");
}

void ReverseEngineeringWidget::createDecompilationTab() {
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout();
    
    m_complexityLabel = new QLabel("Complexity: N/A");
    layout->addWidget(m_complexityLabel);
    
    m_decompilationEdit = new QTextEdit();
    m_decompilationEdit->setReadOnly(true);
    m_decompilationEdit->setFont(QFont("Courier", 10));
    layout->addWidget(m_decompilationEdit);
    
    tab->setLayout(layout);
    m_tabWidget->addTab(tab, "Decompilation");
}

void ReverseEngineeringWidget::createStructsTab() {
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout();
    
    // Structs tree
    m_structsTree = new QTreeWidget();
    m_structsTree->setColumnCount(3);
    m_structsTree->setHeaderLabels({"Name", "Size", "Fields"});
    connect(m_structsTree, &QTreeWidget::itemSelected,
            this, [this](QTreeWidgetItem* item) { updateStructInfo(item->text(0)); });
    layout->addWidget(m_structsTree);
    
    // Code view
    layout->addWidget(new QLabel("Generated Code:"));
    m_structCodeEdit = new QTextEdit();
    m_structCodeEdit->setReadOnly(true);
    m_structCodeEdit->setFont(QFont("Courier", 10));
    layout->addWidget(m_structCodeEdit);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_editStructButton = new QPushButton("Edit Struct");
    buttonLayout->addWidget(m_editStructButton);
    buttonLayout->addStretch();
    layout->addLayout(buttonLayout);
    
    tab->setLayout(layout);
    m_tabWidget->addTab(tab, "Structures");
}

void ReverseEngineeringWidget::createHexTab() {
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout();
    
    // Address input
    QHBoxLayout* addrLayout = new QHBoxLayout();
    addrLayout->addWidget(new QLabel("Address:"));
    m_hexAddressEdit = new QLineEdit();
    m_hexAddressEdit->setPlaceholderText("e.g., 0x400000 or 400000");
    addrLayout->addWidget(m_hexAddressEdit);
    m_hexSearchButton = new QPushButton("Go To");
    connect(m_hexSearchButton, &QPushButton::clicked, this, [this]() {
        QString addrStr = m_hexAddressEdit->text();
        bool ok;
        uint64_t address = addrStr.startsWith("0x") || addrStr.startsWith("0X")
            ? addrStr.mid(2).toULongLong(&ok, 16)
            : addrStr.toULongLong(&ok, 16);
        if (ok) {
            updateHexEditor(address, 256);  // Display 256 bytes
        } else {
            QMessageBox::warning(this, "Error", "Invalid address format");
        }
    });
    addrLayout->addWidget(m_hexSearchButton);
    addrLayout->addStretch();
    layout->addLayout(addrLayout);
    
    // Hex view
    m_hexViewEdit = new QTextEdit();
    m_hexViewEdit->setReadOnly(true);
    m_hexViewEdit->setFont(QFont("Courier", 10));
    layout->addWidget(m_hexViewEdit);
    
    tab->setLayout(layout);
    m_tabWidget->addTab(tab, "Hex Editor");
}

void ReverseEngineeringWidget::createStringsTab() {
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout();
    
    // Search
    QHBoxLayout* searchLayout = new QHBoxLayout();
    searchLayout->addWidget(new QLabel("Search:"));
    m_stringSearchEdit = new QLineEdit();
    searchLayout->addWidget(m_stringSearchEdit);
    searchLayout->addStretch();
    layout->addLayout(searchLayout);
    
    // Strings tree
    m_stringsTree = new QTreeWidget();
    m_stringsTree->setColumnCount(3);
    m_stringsTree->setHeaderLabels({"Address", "String", "Type"});
    m_stringsTree->header()->setStretchLastSection(true);
    connect(m_stringsTree, &QTreeWidget::itemSelected,
            this, &ReverseEngineeringWidget::onStringSelected);
    layout->addWidget(m_stringsTree);
    
    tab->setLayout(layout);
    m_tabWidget->addTab(tab, "Strings");
}

void ReverseEngineeringWidget::createReportsTab() {
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout();
    
    // Report format selector
    QHBoxLayout* formatLayout = new QHBoxLayout();
    formatLayout->addWidget(new QLabel("Format:"));
    QComboBox* formatCombo = new QComboBox();
    formatCombo->addItems({"HTML", "Markdown", "JSON", "CSV", "Plain Text"});
    formatLayout->addWidget(formatCombo);
    formatLayout->addStretch();
    layout->addLayout(formatLayout);
    
    // Report preview
    m_reportEdit = new QTextEdit();
    m_reportEdit->setReadOnly(true);
    layout->addWidget(m_reportEdit);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_generateReportButton = new QPushButton("Generate Report");
    connect(m_generateReportButton, &QPushButton::clicked, this, [this, formatCombo]() {
        if (!m_loader || !m_disassembler || !m_decompiler) {
            QMessageBox::warning(this, "Error", "No binary loaded");
            return;
        }
        
        m_statusLabel->setText("Generating report...");
        
        ReportFormat format = ReportFormat::HTML;
        QString formatStr = formatCombo->currentText();
        if (formatStr == "Markdown") format = ReportFormat::MARKDOWN;
        else if (formatStr == "JSON") format = ReportFormat::JSON;
        else if (formatStr == "CSV") format = ReportFormat::CSV;
        else if (formatStr == "Plain Text") format = ReportFormat::PLAIN_TEXT;
        
        ReportGenerator generator;
        QString report = generator.generateReport(*m_loader, *m_symbolAnalyzer, 
                                                   *m_disassembler, *m_decompiler, format);
        
        m_reportEdit->setText(report);
        m_statusLabel->setText("Report generated");
    });
    m_exportButton = new QPushButton("Export Results");
    connect(m_exportButton, &QPushButton::clicked, this, &ReverseEngineeringWidget::onExportResults);
    buttonLayout->addWidget(m_generateReportButton);
    buttonLayout->addWidget(m_exportButton);
    buttonLayout->addStretch();
    layout->addLayout(buttonLayout);
    
    tab->setLayout(layout);
    m_tabWidget->addTab(tab, "Reports");
}

bool ReverseEngineeringWidget::openBinary(const QString& filePath) {
    if (filePath.isEmpty()) {
        return false;
    }
    
    m_currentBinaryPath = filePath;
    m_binaryPathLabel->setText("Path: " + filePath);
    
    // Load binary
    m_loader = std::make_unique<BinaryLoader>(filePath);
    if (!m_loader->load()) {
        QMessageBox::critical(this, "Error", "Failed to load binary: " + m_loader->errorMessage());
        return false;
    }
    
    emit analysisStarted(filePath);
    m_statusLabel->setText("Loading...");
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    
    // Analyze
    m_symbolAnalyzer = std::make_unique<SymbolAnalyzer>(*m_loader);
    m_progressBar->setValue(15);
    
    m_disassembler = std::make_unique<Disassembler>(*m_loader);
    m_progressBar->setValue(30);
    
    // Analyze code section to find functions
    m_disassembler->analyzeCodeSection(".text");
    m_progressBar->setValue(50);
    
    m_decompiler = std::make_unique<Decompiler>(*m_disassembler);
    m_progressBar->setValue(60);
    
    m_structReconstructor = std::make_unique<StructReconstructor>(*m_decompiler);
    m_progressBar->setValue(70);
    
    m_hexEditor = std::make_unique<HexEditor>();
    m_hexEditor->open(filePath);
    m_progressBar->setValue(80);
    
    m_resourceExtractor = std::make_unique<ResourceExtractor>(*m_loader);
    m_progressBar->setValue(85);
    
    m_symbolManager = std::make_unique<SymbolManager>(*m_symbolAnalyzer);
    m_progressBar->setValue(90);
    
    // Populate UI
    updateBinaryInfo();
    populateSymbolsTree();
    populateFunctionsTree();
    m_resourceExtractor->extractStrings();
    populateStringsTree();
    
    m_progressBar->setValue(100);
    m_statusLabel->setText("Analysis complete!");
    QTimer::singleShot(2000, [this]() { m_progressBar->setVisible(false); });
    
    emit analysisCompleted(true);
    
    qDebug() << "Opened binary:" << filePath;
    return true;
}

void ReverseEngineeringWidget::updateBinaryInfo() {
    if (!m_loader) return;
    
    const auto& metadata = m_loader->metadata();
    m_formatLabel->setText("Format: " + BinaryLoader::formatName(metadata.format));
    m_architectureLabel->setText("Architecture: " + BinaryLoader::architectureName(metadata.architecture));
    m_sectionsLabel->setText(QString("Sections: %1").arg(metadata.sectionCount));
    m_exportsLabel->setText(QString("Exports: %1").arg(metadata.exportCount));
    m_importsLabel->setText(QString("Imports: %1").arg(metadata.importCount));
}

void ReverseEngineeringWidget::populateSymbolsTree() {
    if (!m_symbolAnalyzer) return;
    
    m_symbolsTree->clear();
    
    for (const auto& symbol : m_symbolAnalyzer->symbols()) {
        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setText(0, QString("0x%1").arg(symbol.address, 8, 16, QChar('0')));
        item->setText(1, symbol.name);
        item->setText(2, symbol.type);
        item->setText(3, symbol.moduleName);
        m_symbolsTree->addTopLevelItem(item);
    }
}

void ReverseEngineeringWidget::populateStringsTree() {
    if (!m_resourceExtractor) return;
    
    m_stringsTree->clear();
    
    for (const auto& str : m_resourceExtractor->strings()) {
        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setText(0, QString("0x%1").arg(str.address, 8, 16, QChar('0')));
        item->setText(1, str.value.left(50));
        item->setText(2, str.isUnicode ? "Unicode" : "ASCII");
        m_stringsTree->addTopLevelItem(item);
    }
}

void ReverseEngineeringWidget::onBrowseBinary() {
    QString filePath = QFileDialog::getOpenFileName(this, "Open Binary", "",
                                                     "Executables (*.exe *.dll *.elf *.o);;All files (*)");
    if (!filePath.isEmpty()) {
        openBinary(filePath);
    }
}

void ReverseEngineeringWidget::onAnalyzeNow() {
    if (m_currentBinaryPath.isEmpty()) {
        QMessageBox::warning(this, "Warning", "Please load a binary first");
        return;
    }
    
    openBinary(m_currentBinaryPath);
}

void ReverseEngineeringWidget::onSymbolSelected(QTreeWidgetItem* item) {
    QString details = QString("Name: %1\nAddress: %2\nType: %3").arg(item->text(1), item->text(0), item->text(2));
    m_symbolDetailsEdit->setText(details);
}

void ReverseEngineeringWidget::onFunctionSelected(QTreeWidgetItem* item) {
    updateDisassembly(item->text(0));
}

void ReverseEngineeringWidget::onStringSelected(QTreeWidgetItem* item) {
    // Nothing to do for now
}

void ReverseEngineeringWidget::onRenameSymbol() {
    QTreeWidgetItem* item = m_symbolsTree->currentItem();
    if (!item) {
        QMessageBox::warning(this, "Warning", "Please select a symbol");
        return;
    }
    
    bool ok;
    QString newName = QInputDialog::getText(this, "Rename Symbol", "New name:",
                                           QLineEdit::Normal, item->text(1), &ok);
    if (ok && !newName.isEmpty()) {
        m_symbolManager->renameSymbol(item->text(1), newName);
        item->setText(1, newName);
        QMessageBox::information(this, "Success", "Symbol renamed");
    }
}

void ReverseEngineeringWidget::onAddComment() {
    QTreeWidgetItem* item = m_symbolsTree->currentItem();
    if (!item) {
        QMessageBox::warning(this, "Warning", "Please select a symbol");
        return;
    }
    
    bool ok;
    QString comment = QInputDialog::getMultiLineText(this, "Add Comment", "Comment:",
                                                     "", &ok);
    if (ok && !comment.isEmpty()) {
        m_symbolManager->addComment(item->text(1), comment);
        QMessageBox::information(this, "Success", "Comment added");
    }
}

void ReverseEngineeringWidget::onExportResults() {
    QString filePath = QFileDialog::getSaveFileName(this, "Export Results", "",
                                                     "CSV Files (*.csv);;Headers (*.h);;All files (*)");
    if (!filePath.isEmpty()) {
        if (filePath.endsWith(".csv")) {
            m_symbolManager->exportToCSV(filePath);
        }
        QMessageBox::information(this, "Success", "Results exported");
    }
}

void ReverseEngineeringWidget::onTabChanged(int index) {
    if (m_currentBinaryPath.isEmpty()) {
        return;  // No binary loaded
    }
    
    // Tab-specific initialization
    switch (index) {
        case 0: // Binary Info
            m_statusLabel->setText("Binary Information");
            break;
        case 1: // Symbols
            m_statusLabel->setText("Symbols and Imports/Exports");
            break;
        case 2: // Disassembly
            m_statusLabel->setText("Select a function to disassemble");
            if (m_functionCombo->count() > 0 && m_disassemblyEdit->toPlainText().isEmpty()) {
                m_functionCombo->setCurrentIndex(0);
                updateDisassembly(m_functionCombo->currentText());
            }
            break;
        case 3: // Decompilation
            m_statusLabel->setText("Select a function to decompile");
            if (m_functionCombo->count() > 0 && m_decompilationEdit->toPlainText().isEmpty()) {
                m_functionCombo->setCurrentIndex(0);
                updateDecompilation(m_functionCombo->currentText());
            }
            break;
        case 4: // Structs
            m_statusLabel->setText("Reconstructed Structures");
            if (m_structsTree->topLevelItemCount() == 0 && m_structReconstructor) {
                const auto& structs = m_structReconstructor->structures();
                for (const auto& st : structs) {
                    QTreeWidgetItem* item = new QTreeWidgetItem();
                    item->setText(0, st.name);
                    item->setText(1, QString::number(st.size));
                    item->setText(2, QString::number(st.fields.size()));
                    m_structsTree->addTopLevelItem(item);
                }
            }
            break;
        case 5: // Hex Editor
            m_statusLabel->setText("Hex Editor - Enter address and click 'Go To'");
            break;
        case 6: // Strings
            m_statusLabel->setText(QString("Found %1 strings").arg(m_stringsTree->topLevelItemCount()));
            break;
        case 7: // Reports
            m_statusLabel->setText("Analysis Reports");
            break;
        default:
            break;
    }
    
    qDebug() << "Tab changed to" << index;
}

void ReverseEngineeringWidget::onSearchSymbols(const QString& text) {
    // Filter symbols tree by search text
    for (int i = 0; i < m_symbolsTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_symbolsTree->topLevelItem(i);
        bool match = item->text(1).contains(text, Qt::CaseInsensitive);
        item->setHidden(!match);
    }
}

void ReverseEngineeringWidget::onRefreshAnalysis() {
    if (!m_currentBinaryPath.isEmpty()) {
        openBinary(m_currentBinaryPath);
    }
}

void ReverseEngineeringWidget::updateDisassembly(const QString& functionName) {
    if (!m_disassembler) {
        m_statusLabel->setText("Error: Disassembler not initialized");
        return;
    }
    
    // Find function by name
    FunctionInfo funcInfo = m_disassembler->functionNamed(functionName);
    if (funcInfo.name.isEmpty()) {
        m_disassemblyEdit->setText("Function not found");
        m_xrefsTree->clear();
        return;
    }
    
    m_statusLabel->setText(QString("Disassembling %1...").arg(functionName));
    
    // Generate disassembly output
    QString disassemblyText;
    disassemblyText += QString("%1:\n").arg(functionName);
    disassemblyText += QString("Address: 0x%1, Size: %2 bytes\n").arg(
        funcInfo.address, 8, 16, QChar('0')).arg(funcInfo.size);
    disassemblyText += "---\n\n";
    
    // Add all instructions with syntax highlighting-like formatting
    for (const auto& instr : funcInfo.instructions) {
        // Format address
        QString addrStr = QString("  %1: ").arg(instr.address, 16, 16, QChar('0'));
        
        // Format bytes
        QString bytesStr;
        for (unsigned char byte : instr.bytes) {
            bytesStr += QString("%1 ").arg(byte, 2, 16, QChar('0'));
        }
        bytesStr = bytesStr.leftJustified(24);
        
        // Format mnemonic and operands
        QString instrStr = instr.mnemonic;
        if (!instr.operands.isEmpty()) {
            instrStr += " " + instr.operands;
        }
        
        // Add comment if exists
        if (!instr.comment.isEmpty()) {
            instrStr += QString(" ; %1").arg(instr.comment);
        }
        
        // Highlight branch/call instructions
        if (instr.isBranch || instr.isCall) {
            instrStr += QString(" -> 0x%1").arg(instr.target, 8, 16, QChar('0'));
        }
        
        disassemblyText += addrStr + bytesStr + instrStr + "\n";
    }
    
    m_disassemblyEdit->setText(disassemblyText);
    
    // Populate cross-references
    m_xrefsTree->clear();
    QVector<Instruction> xrefs = m_disassembler->crossReferences(funcInfo.address);
    
    for (const auto& xref : xrefs) {
        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setText(0, QString("0x%1").arg(xref.address, 8, 16, QChar('0')));
        item->setText(1, xref.disassembly);
        m_xrefsTree->addTopLevelItem(item);
    }
    
    m_statusLabel->setText(QString("Disassembled %1 (%2 instructions)")
        .arg(functionName).arg(funcInfo.instructions.size()));
}

void ReverseEngineeringWidget::updateDecompilation(const QString& functionName) {
    if (!m_decompiler) {
        m_statusLabel->setText("Error: Decompiler not initialized");
        return;
    }
    
    m_statusLabel->setText(QString("Decompiling %1...").arg(functionName));
    
    // Find the function first through disassembler
    if (!m_disassembler) {
        m_decompilationEdit->setText("Error: No disassembler available");
        return;
    }
    
    FunctionInfo funcInfo = m_disassembler->functionNamed(functionName);
    if (funcInfo.name.isEmpty()) {
        m_decompilationEdit->setText("Function not found");
        return;
    }
    
    // Decompile the function
    DecompiledFunction decompFunc = m_decompiler->decompile(funcInfo);
    
    // Generate output with syntax highlighting-like formatting
    QString decompText;
    
    // Function signature
    decompText += "// Function signature (inferred)\n";
    decompText += decompFunc.signature + "\n";
    decompText += "// Address: 0x" + QString::number(decompFunc.address, 16).toUpper() + "\n";
    decompText += QString("// Complexity: %1\n").arg(decompFunc.complexity);
    decompText += QString("// Has loops: %1\n\n").arg(decompFunc.hasLoops ? "Yes" : "No");
    
    // Local variables
    if (!decompFunc.variables.isEmpty()) {
        decompText += "// Local variables\n";
        for (const auto& var : decompFunc.variables) {
            decompText += QString("// %1 %2").arg(var.type, var.name);
            if (var.stackOffset != -1) {
                decompText += QString(" [rsp + 0x%1]").arg(var.stackOffset, 2, 16, QChar('0'));
            }
            decompText += "\n";
        }
        decompText += "\n";
    }
    
    // Function body (pseudo-code)
    decompText += decompFunc.pseudoCode;
    
    m_decompilationEdit->setText(decompText);
    m_complexityLabel->setText(QString("Complexity: %1").arg(decompFunc.complexity));
    
    m_statusLabel->setText(QString("Decompiled %1 (Complexity: %2)")
        .arg(functionName).arg(decompFunc.complexity));
}

void ReverseEngineeringWidget::updateStructInfo(const QString& structName) {
    if (!m_structReconstructor) {
        m_statusLabel->setText("Error: Struct reconstructor not initialized");
        return;
    }
    
    m_statusLabel->setText(QString("Loading struct: %1...").arg(structName));
    
    auto structInfo = m_structReconstructor->findStruct(structName);
    if (structInfo.name.isEmpty()) {
        m_structCodeEdit->setText(QString("Struct '%1' not found").arg(structName));
        return;
    }
    
    // Generate formatted struct code
    QString code;
    code += QString("// Struct: %1\n").arg(structInfo.name);
    code += QString("// Size: %1 bytes\n").arg(structInfo.size);
    code += QString("// Inferred from decompilation\n");
    code += "typedef struct {\n";
    
    // Add fields
    for (const auto& field : structInfo.fields) {
        code += QString("    %1 %2;  // 0x%3\n")
            .arg(field.type, -20)
            .arg(field.name, -30)
            .arg(field.offset, 2, 16, QChar('0'));
    }
    
    code += "} " + structInfo.name + ";\n\n";
    code += structInfo.sourceCode;
    
    m_structCodeEdit->setText(code);
    m_statusLabel->setText(QString("Displayed struct: %1 (%2 bytes, %3 fields)")
        .arg(structInfo.name).arg(structInfo.size).arg(structInfo.fields.size()));
}

void ReverseEngineeringWidget::updateHexEditor(uint64_t address, size_t size) {
    if (!m_hexEditor) {
        m_statusLabel->setText("Error: Hex editor not initialized");
        return;
    }
    
    m_statusLabel->setText(QString("Loading hex at 0x%1...").arg(address, 8, 16, QChar('0')));
    
    // Read bytes from hex editor
    QByteArray bytes = m_hexEditor->bytesAt(address, size);
    
    if (bytes.isEmpty()) {
        m_hexViewEdit->setText(QString("No data at address 0x%1").arg(address, 8, 16, QChar('0')));
        return;
    }
    
    // Format as hex dump
    QString hexOutput;
    hexOutput += QString("Address: 0x%1, Size: %2 bytes\n").arg(
        address, 8, 16, QChar('0')).arg(size);
    hexOutput += "---\n";
    
    const int bytesPerLine = 16;
    
    for (size_t i = 0; i < bytes.size(); i += bytesPerLine) {
        // Address
        uint64_t lineAddr = address + i;
        hexOutput += QString("0x%1: ").arg(lineAddr, 8, 16, QChar('0'));
        
        // Hex bytes
        QString hexPart;
        QString asciiPart;
        
        for (int j = 0; j < bytesPerLine && (i + j) < (size_t)bytes.size(); ++j) {
            unsigned char byte = bytes[i + j];
            hexPart += QString("%1 ").arg(byte, 2, 16, QChar('0'));
            asciiPart += (byte >= 32 && byte < 127) ? QChar(byte) : QChar('.');
        }
        
        hexOutput += hexPart.leftJustified(3 * bytesPerLine);
        hexOutput += " | " + asciiPart + "\n";
    }
    
    m_hexViewEdit->setText(hexOutput);
    m_statusLabel->setText(QString("Hex display: 0x%1 (%2 bytes)")
        .arg(address, 8, 16, QChar('0')).arg(size));
}

void ReverseEngineeringWidget::populateFunctionsTree() {
    if (!m_disassembler) return;
    
    m_functionCombo->clear();
    
    const auto& functions = m_disassembler->functions();
    
    // Populate from disassembler's found functions
    for (const auto& func : functions) {
        QString displayName = func.name.isEmpty() 
            ? QString("func_0x%1").arg(func.address, 8, 16, QChar('0'))
            : func.name;
        m_functionCombo->addItem(displayName);
    }
    
    // Also add exported functions from loader if available
    if (m_loader) {
        const auto& exports = m_loader->exports();
        for (const auto& exp : exports) {
            if (!exp.name.isEmpty() && m_functionCombo->findText(exp.name) == -1) {
                m_functionCombo->addItem(exp.name);
            }
        }
    }
    
    if (m_functionCombo->count() > 0) {
        m_statusLabel->setText(QString("Loaded %1 functions").arg(m_functionCombo->count()));
    } else {
        m_statusLabel->setText("No functions found");
    }
}
