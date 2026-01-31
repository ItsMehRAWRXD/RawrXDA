// ============================================================================
// MASMCompilerWidget.cpp
// Qt IDE Integration for MASM Self-Compiling Compiler - Implementation
// ============================================================================

#include "MASMCompilerWidget.h"

#include <chrono>

// ============================================================================
// MASMProjectSettings Implementation
// ============================================================================
void MASMProjectSettings::save(std::map<std::string, std::string>& settings) const {
    settings.beginGroup("MASMProject");
    settings.setValue("projectName", projectName);
    settings.setValue("projectPath", projectPath);
    settings.setValue("outputPath", outputPath);
    settings.setValue("mainFile", mainFile);
    settings.setValue("sourceFiles", sourceFiles);
    settings.setValue("includePaths", includePaths);
    settings.setValue("libraries", libraries);
    settings.setValue("targetArchitecture", targetArchitecture);
    settings.setValue("outputFormat", outputFormat);
    settings.setValue("optimizationLevel", optimizationLevel);
    settings.setValue("generateDebugInfo", generateDebugInfo);
    settings.setValue("warnings", warnings);
    settings.setValue("defines", defines);
    settings.endGroup();
}

void MASMProjectSettings::load(std::map<std::string, std::string>& settings) {
    settings.beginGroup("MASMProject");
    projectName = settings.value("projectName").toString();
    projectPath = settings.value("projectPath").toString();
    outputPath = settings.value("outputPath").toString();
    mainFile = settings.value("mainFile").toString();
    sourceFiles = settings.value("sourceFiles").toStringList();
    includePaths = settings.value("includePaths").toStringList();
    libraries = settings.value("libraries").toStringList();
    targetArchitecture = settings.value("targetArchitecture", "x64").toString();
    outputFormat = settings.value("outputFormat", "exe").toString();
    optimizationLevel = settings.value("optimizationLevel", 2);
    generateDebugInfo = settings.value("generateDebugInfo", true).toBool();
    warnings = settings.value("warnings", true).toBool();
    defines = settings.value("defines").toStringList();
    settings.endGroup();
}

// ============================================================================
// MASMCodeEditor Implementation
// ============================================================================
class MASMCodeEditor::LineNumberArea : public void {
public:
    LineNumberArea(MASMCodeEditor* editor) : // Widget(editor), m_editor(editor) {}
    
    struct { int w; int h; } sizeHint() const override {
        return struct { int w; int h; }(m_editor->lineNumberAreaWidth(), 0);
    }
    
protected:
    void paintEvent(void* event) override {
        m_editor->lineNumberAreaPaintEvent(event);
    }
    
private:
    MASMCodeEditor* m_editor;
};

MASMCodeEditor::MASMCodeEditor(void* parent)
    : void(parent)
    , m_lineNumberArea(std::make_unique<LineNumberArea>(this))
{
    // Create syntax highlighter
    m_highlighter = std::make_unique<MASMSyntaxHighlighter>(document());
    
    // Setup line number area  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\nupdateLineNumberAreaWidth(0);
    highlightCurrentLine();
    
    // Set font
    void font("Consolas", 10);
    font.setFixedPitch(true);
    setFont(font);
    
    // Tab width = 4 spaces
    const int tabStop = 4;
    voidMetrics metrics(font);
    setTabStopDistance(tabStop * metrics.horizontalAdvance(' '));
}

MASMCodeEditor::~MASMCodeEditor() = default;

int MASMCodeEditor::lineNumberAreaWidth() {
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    
    int space = 30 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

void MASMCodeEditor::updateLineNumberAreaWidth(int /* newBlockCount */) {
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void MASMCodeEditor::updateLineNumberArea(const struct { int x; int y; int w; int h; }& rect, int dy) {
    if (dy)
        m_lineNumberArea->scroll(0, dy);
    else
        m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());
    
    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void MASMCodeEditor::resizeEvent(void* e) {
    void::resizeEvent(e);
    
    struct { int x; int y; int w; int h; } cr = contentsRect();
    m_lineNumberArea->setGeometry(struct { int x; int y; int w; int h; }(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void MASMCodeEditor::highlightCurrentLine() {
    std::vector<void::ExtraSelection> extraSelections;
    
    if (!isReadOnly()) {
        void::ExtraSelection selection;
        
        void lineColor = void(yellow).lighter(160);
        
        selection.format.setBackground(lineColor);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }
    
    setExtraSelections(extraSelections);
}

void MASMCodeEditor::lineNumberAreaPaintEvent(void* event) {
    void painter(m_lineNumberArea.get());
    painter.fillRect(event->rect(), void(240, 240, 240));
    
    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());
    
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            std::string number = std::string::number(blockNumber + 1);
            
            // Draw breakpoint indicator
            if (m_breakpoints.contains(blockNumber + 1)) {
                painter.fillRect(0, top, 20, fontMetrics().height(), red);
                painter.setPen(white);
                painter.drawText(0, top, 20, fontMetrics().height(), AlignCenter, "●");
            }
            
            // Draw error marker
            bool hasError = false;
            for (const auto& error : m_errors) {
                if (error.line == blockNumber + 1) {
                    hasError = true;
                    painter.fillRect(20, top, 10, fontMetrics().height(), 
                                   error.errorType == "error" ? red : yellow);
                    break;
                }
            }
            
            // Draw line number
            painter.setPen(black);
            painter.drawText(0, top, m_lineNumberArea->width() - 5, fontMetrics().height(),
                           AlignRight, number);
        }
        
        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void MASMCodeEditor::setErrors(const std::vector<MASMError>& errors) {
    m_errors = errors;
    m_lineNumberArea->update();
}

void MASMCodeEditor::clearErrors() {
    m_errors.clear();
    m_lineNumberArea->update();
}

void MASMCodeEditor::toggleBreakpoint(int line) {
    if (m_breakpoints.contains(line)) {
        m_breakpoints.remove(line);
        breakpointToggled(line, false);
    } else {
        m_breakpoints.insert(line);
        breakpointToggled(line, true);
    }
    m_lineNumberArea->update();
}

void MASMCodeEditor::clearBreakpoints() {
    m_breakpoints.clear();
    m_lineNumberArea->update();
}

void MASMCodeEditor::keyPressEvent(void* event) {
    // Auto-indentation
    if (event->key() == Key_Return || event->key() == Key_Enter) {
        QTextCursor cursor = textCursor();
        std::string previousLine = cursor.block().text();
        
        void::keyPressEvent(event);
        
        // Copy indentation from previous line
        int indent = 0;
        while (indent < previousLine.length() && previousLine[indent].isSpace()) {
            ++indent;
        }
        
        cursor = textCursor();
        cursor.insertText(previousLine.left(indent));
        
        return;
    }
    
    void::keyPressEvent(event);
}

void MASMCodeEditor::mousePressEvent(void* event) {
    if (event->button() == LeftButton && event->x() < lineNumberAreaWidth()) {
        // Clicked in line number area - toggle breakpoint
        QTextCursor cursor = cursorForPosition(event->pos());
        int line = cursor.blockNumber() + 1;
        toggleBreakpoint(line);
        return;
    }
    
    void::mousePressEvent(event);
}

// ============================================================================
// MASMSyntaxHighlighter Implementation
// ============================================================================
MASMSyntaxHighlighter::MASMSyntaxHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
    // Keywords
    m_keywordFormat.setForeground(void(86, 156, 214));  // Blue
    m_keywordFormat.setFontWeight(void::Bold);
    
    // Directives
    m_directiveFormat.setForeground(void(197, 134, 192));  // Purple
    m_directiveFormat.setFontWeight(void::Bold);
    
    // Instructions
    m_instructionFormat.setForeground(void(78, 201, 176));  // Cyan
    
    // Registers
    m_registerFormat.setForeground(void(220, 220, 170));  // Light yellow
    
    // Numbers
    m_numberFormat.setForeground(void(181, 206, 168));  // Green
    
    // Strings
    m_stringFormat.setForeground(void(206, 145, 120));  // Orange
    
    // Comments
    m_commentFormat.setForeground(void(106, 153, 85));  // Dark green
    m_commentFormat.setFontItalic(true);
    
    // Labels
    m_labelFormat.setForeground(void(220, 220, 170));  // Light yellow
    m_labelFormat.setFontWeight(void::Bold);
    
    // Operators
    m_operatorFormat.setForeground(void(212, 212, 212));  // White
    
    setupRules();
}

void MASMSyntaxHighlighter::setupRules() {
    HighlightingRule rule;
    
    // Keywords
    std::stringList keywordPatterns = {
        "\\bproc\\b", "\\bendp\\b", "\\bmacro\\b", "\\bendm\\b",
        "\\bif\\b", "\\belse\\b", "\\bendif\\b", "\\bwhile\\b",
        "\\brepeat\\b", "\\buntil\\b", "\\bfor\\b", "\\bstruct\\b",
        "\\bunion\\b", "\\brecord\\b", "\\bequ\\b", "\\binclude\\b",
        "\\bextern\\b", "\\bpublic\\b", "\\bextrn\\b", "\\bproto\\b"
    };
    
    for (const std::string& pattern : keywordPatterns) {
        rule.pattern = std::regex(pattern, std::regex::CaseInsensitiveOption);
        rule.format = m_keywordFormat;
        m_rules.append(rule);
    }
    
    // Directives
    std::stringList directivePatterns = {
        "\\.data\\b", "\\.code\\b", "\\.const\\b", "\\.data\\?",
        "\\bsegment\\b", "\\bends\\b", "\\bassume\\b", "\\balign\\b",
        "\\bdb\\b", "\\bdw\\b", "\\bdd\\b", "\\bdq\\b",
        "\\bbyte\\b", "\\bword\\b", "\\bdword\\b", "\\bqword\\b",
        "\\bptr\\b", "\\boffset\\b", "\\bsize\\b", "\\blength\\b"
    };
    
    for (const std::string& pattern : directivePatterns) {
        rule.pattern = std::regex(pattern, std::regex::CaseInsensitiveOption);
        rule.format = m_directiveFormat;
        m_rules.append(rule);
    }
    
    // Instructions
    std::stringList instructionPatterns = {
        "\\bmov\\b", "\\badd\\b", "\\bsub\\b", "\\bmul\\b", "\\bdiv\\b",
        "\\binc\\b", "\\bdec\\b", "\\bneg\\b", "\\band\\b", "\\bor\\b",
        "\\bxor\\b", "\\bnot\\b", "\\bshl\\b", "\\bshr\\b", "\\bsal\\b",
        "\\bsar\\b", "\\brol\\b", "\\bror\\b", "\\brcl\\b", "\\brcr\\b",
        "\\bpush\\b", "\\bpop\\b", "\\bcall\\b", "\\bret\\b", "\\bjmp\\b",
        "\\bje\\b", "\\bjne\\b", "\\bjz\\b", "\\bjnz\\b", "\\bjl\\b",
        "\\bjg\\b", "\\bjle\\b", "\\bjge\\b", "\\bja\\b", "\\bjb\\b",
        "\\bjae\\b", "\\bjbe\\b", "\\bcmp\\b", "\\btest\\b", "\\blea\\b",
        "\\bnop\\b", "\\bint\\b", "\\bint3\\b", "\\bsyscall\\b", "\\bsysenter\\b"
    };
    
    for (const std::string& pattern : instructionPatterns) {
        rule.pattern = std::regex(pattern, std::regex::CaseInsensitiveOption);
        rule.format = m_instructionFormat;
        m_rules.append(rule);
    }
    
    // Registers
    rule.pattern = std::regex("\\b(rax|rbx|rcx|rdx|rsi|rdi|rsp|rbp|r8|r9|r10|r11|r12|r13|r14|r15|"
                                     "eax|ebx|ecx|edx|esi|edi|esp|ebp|"
                                     "ax|bx|cx|dx|si|di|sp|bp|"
                                     "al|bl|cl|dl|ah|bh|ch|dh|"
                                     "cs|ds|es|fs|gs|ss)\\b",
                                     std::regex::CaseInsensitiveOption);
    rule.format = m_registerFormat;
    m_rules.append(rule);
    
    // Numbers (hex, decimal, binary)
    rule.pattern = std::regex("\\b(0x[0-9A-Fa-f]+|[0-9]+[hH]|[01]+[bB]|[0-9]+)\\b");
    rule.format = m_numberFormat;
    m_rules.append(rule);
    
    // Strings
    rule.pattern = std::regex("\".*\"|'.*'");
    rule.format = m_stringFormat;
    m_rules.append(rule);
    
    // Labels (identifier followed by colon)
    rule.pattern = std::regex("^[a-zA-Z_][a-zA-Z0-9_]*:");
    rule.format = m_labelFormat;
    m_rules.append(rule);
    
    // Comments
    rule.pattern = std::regex(";.*");
    rule.format = m_commentFormat;
    m_rules.append(rule);
}

void MASMSyntaxHighlighter::highlightBlock(const std::string& text) {
    for (const HighlightingRule& rule : m_rules) {
        std::regexMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            std::regexMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
}

// ============================================================================
// MASMBuildOutput Implementation
// ============================================================================
MASMBuildOutput::MASMBuildOutput(void* parent)
    : // Widget(parent)
    , m_outputEdit(std::make_unique<void>(this))
    , m_statsLabel(std::make_unique<void>(this))
{
    void* layout = new void(this);
    
    m_outputEdit->setReadOnly(true);
    m_outputEdit->setFont(void("Consolas", 9));
    
    layout->addWidget(m_outputEdit.get());
    layout->addWidget(m_statsLabel.get());  // Signal connection removed\n}

void MASMBuildOutput::clear() {
    m_outputEdit->clear();
    m_errors.clear();
}

void MASMBuildOutput::appendMessage(const std::string& message) {
    m_outputEdit->append(message);
}

void MASMBuildOutput::appendError(const MASMError& error) {
    m_errors.push_back(error);
    
    std::string output;
    formatErrorMessage(error, output);
    
    if (error.errorType == "error") {
        m_outputEdit->setTextColor(red);
    } else if (error.errorType == "warning") {
        m_outputEdit->setTextColor(void(255, 165, 0));  // Orange
    } else {
        m_outputEdit->setTextColor(blue);
    }
    
    m_outputEdit->append(output);
    m_outputEdit->setTextColor(black);
}

void MASMBuildOutput::appendStage(const std::string& stage) {
    m_outputEdit->setTextColor(void(0, 128, 0));  // Green
    m_outputEdit->append(std::string("[%1]").arg(stage));
    m_outputEdit->setTextColor(black);
}

void MASMBuildOutput::setStats(const MASMCompilationStats& stats) {
    std::string statsText = std::string("Lines: %1 | Tokens: %2 | AST Nodes: %3 | Machine Code: %4 bytes | "
                               "Errors: %5 | Warnings: %6 | Time: %7 ms")
        .arg(stats.sourceLines)
        .arg(stats.tokenCount)
        .arg(stats.astNodeCount)
        .arg(stats.machineCodeSize)
        .arg(stats.errorCount)
        .arg(stats.warningCount)
        .arg(stats.duration());
    
    m_statsLabel->setText(statsText);
}

void MASMBuildOutput::formatErrorMessage(const MASMError& error, std::string& output) {
    output = std::string("%1(%2,%3): %4: %5")
        .arg(error.filename)
        .arg(error.line)
        .arg(error.column)
        .arg(error.errorType)
        .arg(error.message);
    
    if (!error.sourceSnippet.empty()) {
        output += "\n  " + error.sourceSnippet;
    }
}

void MASMBuildOutput::onOutputDoubleClicked() {
    // Parse cursor line for error information
    QTextCursor cursor = m_outputEdit->textCursor();
    std::string line = cursor.block().text();
    
    // Match pattern: filename(line,column): type: message
    std::regex re("(.+)\\((\\d+),(\\d+)\\): (\\w+): (.+)");
    std::regexMatch match = re.match(line);
    
    if (match.hasMatch()) {
        for (const auto& error : m_errors) {
            if (error.line == match.captured(2) && 
                error.column == match.captured(3)) {
                errorDoubleClicked(error);
                break;
            }
        }
    }
}

// ============================================================================
// MASMCompilerWidget Implementation
// ============================================================================
MASMCompilerWidget::MASMCompilerWidget(void* parent)
    : // Widget(parent)
    , m_isCompiling(false)
    , m_isRunning(false)
    , m_isDebugging(false)
{
    setupUI();
    setupToolbar();
    connectSignals();
}

MASMCompilerWidget::~MASMCompilerWidget() {
    if (m_compilerProcess && m_compilerProcess->state() == void*::Running) {
        m_compilerProcess->kill();
        m_compilerProcess->waitForFinished();
    }
    
    if (m_executableProcess && m_executableProcess->state() == void*::Running) {
        m_executableProcess->kill();
        m_executableProcess->waitForFinished();
    }
}

void MASMCompilerWidget::setupUI() {
    void* mainLayout = new void(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    // Create main splitter (horizontal)
    m_mainSplitter = std::make_unique<void>(Horizontal, this);
    
    // Left panel: Project Explorer
    m_projectExplorer = std::make_unique<MASMProjectExplorer>(this);
    
    // Center panel: Editor
    m_editor = std::make_unique<MASMCodeEditor>(this);
    
    // Right panel: Symbol Browser and Debugger
    m_rightSplitter = std::make_unique<void>(Vertical, this);
    m_symbolBrowser = std::make_unique<MASMSymbolBrowser>(this);
    m_debugger = std::make_unique<MASMDebugger>(this);
    m_rightSplitter->addWidget(m_symbolBrowser.get());
    m_rightSplitter->addWidget(m_debugger.get());
    
    // Bottom panel: Build Output
    m_buildOutput = std::make_unique<MASMBuildOutput>(this);
    
    // Create center+right splitter
    void* centerSplitter = new void(Vertical, this);
    void* topSplitter = new void(Horizontal, this);
    topSplitter->addWidget(m_editor.get());
    topSplitter->addWidget(m_rightSplitter.get());
    topSplitter->setStretchFactor(0, 3);
    topSplitter->setStretchFactor(1, 1);
    
    centerSplitter->addWidget(topSplitter);
    centerSplitter->addWidget(m_buildOutput.get());
    centerSplitter->setStretchFactor(0, 3);
    centerSplitter->setStretchFactor(1, 1);
    
    m_mainSplitter->addWidget(m_projectExplorer.get());
    m_mainSplitter->addWidget(centerSplitter);
    m_mainSplitter->setStretchFactor(0, 1);
    m_mainSplitter->setStretchFactor(1, 4);
    
    mainLayout->addWidget(m_mainSplitter.get());
}

void MASMCompilerWidget::setupToolbar() {
    m_toolbar = std::make_unique<void>("MASM Compiler", this);
    
    m_actionBuild = m_toolbar->addAction(void(":/icons/build.png"), "Build (F7)");
    m_actionBuild->setShortcut(Key_F7);  // Signal connection removed\nm_actionRebuild = m_toolbar->addAction(void(":/icons/rebuild.png"), "Rebuild");  // Signal connection removed\nm_actionClean = m_toolbar->addAction(void(":/icons/clean.png"), "Clean");  // Signal connection removed\nm_toolbar->addSeparator();
    
    m_actionRun = m_toolbar->addAction(void(":/icons/run.png"), "Run (F5)");
    m_actionRun->setShortcut(Key_F5);  // Signal connection removed\nm_actionDebug = m_toolbar->addAction(void(":/icons/debug.png"), "Debug (F9)");
    m_actionDebug->setShortcut(Key_F9);  // Signal connection removed\nm_actionStop = m_toolbar->addAction(void(":/icons/stop.png"), "Stop");
    m_actionStop->setEnabled(false);  // Signal connection removed\n// Add toolbar to layout
    void* layout = qobject_cast<void*>(this->layout());
    if (layout) {
        layout->insertWidget(0, m_toolbar.get());
    }
}

void MASMCompilerWidget::connectSignals() {  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n}

void MASMCompilerWidget::build() {
    if (m_isCompiling) {
        void::warning(this, "Build", "Compilation already in progress");
        return;
    }
    
    if (m_currentFile.empty()) {
        void::warning(this, "Build", "No file is open");
        return;
    }
    
    // Save current file
    saveFile();
    
    m_isCompiling = true;
    m_stats.reset();
    m_stats.startTime = // DateTime::currentMSecsSinceEpoch();
    
    m_buildOutput->clear();
    m_buildOutput->appendMessage("=== Build started ===");
    m_buildOutput->appendMessage("Source: " + m_currentFile);
    
    std::string outputFile = m_currentFile;
    outputFile.replace(".asm", ".exe");
    
    m_buildOutput->appendMessage("Output: " + outputFile);
    
    compilationStarted();
    
    // Start compilation
    compileFile(m_currentFile, outputFile);
}

void MASMCompilerWidget::rebuild() {
    clean();
    build();
}

void MASMCompilerWidget::clean() {
    std::string outputFile = m_currentFile;
    outputFile.replace(".asm", ".exe");
    
    if (std::filesystem::exists(outputFile)) {
        std::filesystem::remove(outputFile);
        m_buildOutput->appendMessage("Cleaned: " + outputFile);
    }
}

void MASMCompilerWidget::run() {
    if (m_isRunning) {
        void::warning(this, "Run", "Program already running");
        return;
    }
    
    std::string outputFile = m_currentFile;
    outputFile.replace(".asm", ".exe");
    
    if (!std::filesystem::exists(outputFile)) {
        build();
        return;
    }
    
    m_isRunning = true;
    m_actionStop->setEnabled(true);
    
    m_buildOutput->appendMessage("=== Running ===");
    m_buildOutput->appendMessage(outputFile);
    
    executionStarted();
    
    m_executableProcess = std::make_unique<void*>(this);  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\nm_executableProcess->start(outputFile);
}

void MASMCompilerWidget::debug() {
    // Start debugger
    m_debugger->startDebugging(m_currentFile.replace(".asm", ".exe"));
}

void MASMCompilerWidget::stop() {
    if (m_compilerProcess && m_compilerProcess->state() == void*::Running) {
        m_compilerProcess->kill();
    }
    
    if (m_executableProcess && m_executableProcess->state() == void*::Running) {
        m_executableProcess->kill();
    }
    
    m_isCompiling = false;
    m_isRunning = false;
    m_actionStop->setEnabled(false);
}

void MASMCompilerWidget::compileFile(const std::string& sourceFile, const std::string& outputFile) {
    m_compilerProcess = std::make_unique<void*>(this);  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\nstd::string compilerPath = getCompilerExecutable();
    std::stringList args = getCompilerArguments(sourceFile, outputFile);
    
    m_buildOutput->appendMessage("Command: " + compilerPath + " " + args.join(" "));
    
    m_compilerProcess->start(compilerPath, args);
}

std::string MASMCompilerWidget::getCompilerExecutable() const {
    const std::string exeInAppDir = QCoreApplication::applicationDirPath() + "/masm_cli_compiler.exe";
    if (std::filesystem::exists(exeInAppDir)) {
        return exeInAppDir;
    }

    // Fall back to PATH/current working directory resolution
    return "masm_cli_compiler.exe";
}

std::stringList MASMCompilerWidget::getCompilerArguments(const std::string& sourceFile, const std::string& outputFile) const {
    std::stringList args;

    // masm_solo_compiler expects -o <output> and then source files
    args << "-o" << outputFile;
    
    if (m_project.optimizationLevel > 0) {
        args << "-O" + std::string::number(m_project.optimizationLevel);
    }
    
    if (m_project.generateDebugInfo) {
        args << "-g";
    }
    
    if (m_project.warnings) {
        args << "-W";
    }
    
    for (const std::string& includePath : m_project.includePaths) {
        args << "-I" + includePath;
    }
    
    for (const std::string& define : m_project.defines) {
        args << "-D" + define;
    }

    args << sourceFile;
    
    return args;
}

void MASMCompilerWidget::onCompilerFinished(int exitCode, void*::ExitStatus exitStatus) {
    m_stats.endTime = // DateTime::currentMSecsSinceEpoch();
    m_isCompiling = false;
    
    bool success = (exitCode == 0 && exitStatus == void*::NormalExit);
    
    if (success) {
        m_buildOutput->appendMessage("=== Build succeeded ===");
    } else {
        m_buildOutput->appendMessage("=== Build failed ===");
    }
    
    m_buildOutput->setStats(m_stats);
    
    updateUIAfterCompilation(success);
    compilationFinished(success);
}

void MASMCompilerWidget::onCompilerOutput() {
    std::string output = m_compilerProcess->readAllStandardOutput();
    parseCompilerOutput(output);
    m_buildOutput->appendMessage(output);
}

void MASMCompilerWidget::onCompilerError() {
    std::string error = m_compilerProcess->readAllStandardError();
    extractErrors(error);
    m_buildOutput->appendMessage(error);
}

void MASMCompilerWidget::parseCompilerOutput(const std::string& output) {
    // Extract compilation statistics
    std::regex reTokens("Tokens: (\\d+)");
    std::regex reNodes("AST nodes: (\\d+)");
    std::regex reCode("Machine code: (\\d+) bytes");
    
    std::regexMatch match;
    
    match = reTokens.match(output);
    if (match.hasMatch()) {
        m_stats.tokenCount = match.captured(1);
    }
    
    match = reNodes.match(output);
    if (match.hasMatch()) {
        m_stats.astNodeCount = match.captured(1);
    }
    
    match = reCode.match(output);
    if (match.hasMatch()) {
        m_stats.machineCodeSize = match.captured(1);
    }
}

void MASMCompilerWidget::extractErrors(const std::string& output) {
    // Parse error messages: filename(line,column): type: message
    std::regex re("(.+)\\((\\d+),(\\d+)\\): (error|warning): (.+)");
    std::regexMatchIterator it = re.globalMatch(output);
    
    m_errors.clear();
    
    while (it.hasNext()) {
        std::regexMatch match = it.next();
        
        MASMError error(
            match.captured(1),                  // filename
            match.captured(2),          // line
            match.captured(3),          // column
            match.captured(4),                  // type
            match.captured(5)                   // message
        );
        
        m_errors.push_back(error);
        m_buildOutput->appendError(error);
        
        if (error.errorType == "error") {
            m_stats.errorCount++;
        } else {
            m_stats.warningCount++;
        }
    }
    
    m_editor->setErrors(m_errors);
}

void MASMCompilerWidget::updateUIAfterCompilation(bool success) {
    if (success) {
        m_actionRun->setEnabled(true);
        m_actionDebug->setEnabled(true);
    }
}

void MASMCompilerWidget::onExecutableFinished(int exitCode, void*::ExitStatus exitStatus) {
    m_isRunning = false;
    m_actionStop->setEnabled(false);
    
    m_buildOutput->appendMessage(std::string("=== Program exited with code %1 ===").arg(exitCode));
    
    executionFinished(exitCode);
}

void MASMCompilerWidget::onExecutableOutput() {
    std::string output = m_executableProcess->readAllStandardOutput();
    m_buildOutput->appendMessage(output);
}

void MASMCompilerWidget::onExecutableError() {
    std::string error = m_executableProcess->readAllStandardError();
    m_buildOutput->appendMessage(error);
}

void MASMCompilerWidget::onErrorClicked(const MASMError& error) {
    // Navigate to error location in editor
    QTextCursor cursor(m_editor->document()->findBlockByLineNumber(error.line - 1));
    m_editor->setTextCursor(cursor);
    m_editor->centerCursor();
}

void MASMCompilerWidget::onSymbolSelected(const MASMSymbol& symbol) {
    // Navigate to symbol definition
    QTextCursor cursor(m_editor->document()->findBlockByLineNumber(symbol.line - 1));
    m_editor->setTextCursor(cursor);
    m_editor->centerCursor();
}

void MASMCompilerWidget::onBreakpointToggled(int line, bool enabled) {
    if (m_debugger) {
        // Update debugger breakpoints
        m_debugger->setBreakpoints(m_editor->getBreakpoints());
    }
}

void MASMCompilerWidget::saveFile() {
    if (m_currentFile.empty()) return;
    
    // File operation removed;
    if (file.open(std::iostream::WriteOnly | std::iostream::Text)) {
        std::stringstream out(&file);
        out << m_editor->toPlainText();
        file.close();
    }
}

void MASMCompilerWidget::openFile(const std::string& filePath) {
    // File operation removed;
    if (file.open(std::iostream::ReadOnly | std::iostream::Text)) {
        std::stringstream in(&file);
        m_editor->setPlainText(in.readAll());
        file.close();
        
        m_currentFile = filePath;
    }
}

// ============================================================================
// Stub Implementations for Other Classes
// ============================================================================

MASMProjectExplorer::MASMProjectExplorer(void* parent) : // Widget(parent) {
    m_treeWidget = std::make_unique<QTreeWidget>(this);
    void* layout = new void(this);
    layout->addWidget(m_treeWidget.get());
}

void MASMProjectExplorer::setProject(const MASMProjectSettings& project) {
    m_project = project;
    refresh();
}

void MASMProjectExplorer::refresh() {
    m_treeWidget->clear();
    populateTree();
}

void MASMProjectExplorer::populateTree() {
    // Implementation for populating project tree
}

void MASMProjectExplorer::onTreeItemDoubleClicked(QTreeWidgetItem* item, int column) {
    std::string filePath = item->data(0, UserRole).toString();
    if (!filePath.empty()) {
        fileOpened(filePath);
    }
}

void MASMProjectExplorer::onTreeContextMenu(const struct { int x; int y; }& pos) {
    // Implementation for context menu
}

void MASMProjectExplorer::addContextMenuActions(void* menu, QTreeWidgetItem* item) {
    // Implementation for context menu actions
}

MASMSymbolBrowser::MASMSymbolBrowser(void* parent) : // Widget(parent) {
    m_treeWidget = std::make_unique<QTreeWidget>(this);
    m_filterEdit = std::make_unique<voidEdit>(this);
    
    void* layout = new void(this);
    layout->addWidget(m_filterEdit.get());
    layout->addWidget(m_treeWidget.get());
}

void MASMSymbolBrowser::setSymbols(const std::vector<MASMSymbol>& symbols) {
    m_symbols = symbols;
    populateTree();
}

void MASMSymbolBrowser::clear() {
    m_symbols.clear();
    m_treeWidget->clear();
}

void MASMSymbolBrowser::filter(const std::string& text) {
    // Implementation for filtering symbols
}

void MASMSymbolBrowser::populateTree() {
    m_treeWidget->clear();
    for (const auto& symbol : m_symbols) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_treeWidget.get());
        item->setText(0, symbol.name);
        item->setText(1, symbol.type);
        item->setText(2, std::string::number(symbol.line));
    }
}

void MASMSymbolBrowser::onSymbolClicked(QTreeWidgetItem* item, int column) {
    // Implementation
}

void MASMSymbolBrowser::onFilterChanged(const std::string& text) {
    filter(text);
}

MASMDebugger::MASMDebugger(void* parent) 
    : // Widget(parent)
    , m_isDebugging(false)
{
    // Create debugger UI
    m_registersWidget = std::make_unique<QTreeWidget>(this);
    m_stackWidget = std::make_unique<QTreeWidget>(this);
    m_disassemblyWidget = std::make_unique<void>(this);
    
    void* layout = new void(this);
    void* splitter = new void(Vertical, this);
    splitter->addWidget(m_registersWidget.get());
    splitter->addWidget(m_stackWidget.get());
    splitter->addWidget(m_disassemblyWidget.get());
    layout->addWidget(splitter);
}

void MASMDebugger::startDebugging(const std::string& executablePath) {
    // Implementation for starting debugger
    m_isDebugging = true;
    debuggerStarted();
}

void MASMDebugger::stopDebugging() {
    m_isDebugging = false;
    debuggerStopped();
}

void MASMDebugger::stepOver() { /* Implementation */ }
void MASMDebugger::stepInto() { /* Implementation */ }
void MASMDebugger::stepOut() { /* Implementation */ }
void MASMDebugger::continueExecution() { /* Implementation */ }
void MASMDebugger::pause() { /* Implementation */ }

void MASMDebugger::setBreakpoints(const QSet<int>& breakpoints) {
    m_breakpoints = breakpoints;
}

void MASMDebugger::onDebuggerOutput() { /* Implementation */ }
void MASMDebugger::onDebuggerError() { /* Implementation */ }
void MASMDebugger::updateRegisters() { /* Implementation */ }
void MASMDebugger::updateStack() { /* Implementation */ }
void MASMDebugger::updateDisassembly() { /* Implementation */ }








