/**
 * @file snippet_manager_widget.cpp
 * @brief Full Snippet Manager implementation for RawrXD IDE
 * @author RawrXD Team
 */

#include "snippet_manager_widget.h"
#include "integration/ProdIntegration.h"
#include "integration/InitializationTracker.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QRegularExpression>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>

// No namespace - classes are at global scope (matching header)

// =============================================================================
// Snippet Implementation
// =============================================================================

QJsonObject Snippet::toJson() const {
    QJsonObject obj;
    obj["name"] = name;
    obj["prefix"] = prefix;
    obj["description"] = description;
    obj["body"] = body;
    obj["scope"] = scope;
    obj["category"] = category;
    obj["isBuiltIn"] = isBuiltIn;
    obj["isEnabled"] = isEnabled;
    
    QJsonArray keywordsArray;
    for (const QString& kw : keywords) {
        keywordsArray.append(kw);
    }
    obj["keywords"] = keywordsArray;
    
    return obj;
}

Snippet Snippet::fromJson(const QJsonObject& obj) {
    Snippet s;
    s.name = obj["name"].toString();
    s.prefix = obj["prefix"].toString();
    s.description = obj["description"].toString();
    s.body = obj["body"].toString();
    s.scope = obj["scope"].toString();
    s.category = obj["category"].toString();
    s.isBuiltIn = obj["isBuiltIn"].toBool(false);
    s.isEnabled = obj["isEnabled"].toBool(true);
    
    for (const QJsonValue& v : obj["keywords"].toArray()) {
        s.keywords.append(v.toString());
    }
    
    return s;
}

Snippet Snippet::fromVSCodeFormat(const QString& name, const QJsonObject& obj) {
    Snippet s;
    s.name = name;
    s.description = obj["description"].toString();
    s.scope = obj["scope"].toString();
    
    // Handle prefix - can be string or array
    QJsonValue prefixVal = obj["prefix"];
    if (prefixVal.isArray()) {
        QStringList prefixes;
        for (const QJsonValue& v : prefixVal.toArray()) {
            prefixes.append(v.toString());
        }
        s.prefix = prefixes.join(", ");
    } else {
        s.prefix = prefixVal.toString();
    }
    
    // Handle body - can be string or array of lines
    QJsonValue bodyVal = obj["body"];
    if (bodyVal.isArray()) {
        QStringList lines;
        for (const QJsonValue& v : bodyVal.toArray()) {
            lines.append(v.toString());
        }
        s.body = lines.join("\n");
    } else {
        s.body = bodyVal.toString();
    }
    
    return s;
}

QString Snippet::expandBody(const QMap<QString, QString>& variables) const {
    QString result = body;
    
    // Built-in variables
    QMap<QString, QString> allVars = variables;
    if (!allVars.contains("TM_CURRENT_LINE")) allVars["TM_CURRENT_LINE"] = "";
    if (!allVars.contains("TM_CURRENT_WORD")) allVars["TM_CURRENT_WORD"] = "";
    if (!allVars.contains("TM_FILENAME")) allVars["TM_FILENAME"] = "untitled";
    if (!allVars.contains("TM_FILENAME_BASE")) allVars["TM_FILENAME_BASE"] = "untitled";
    if (!allVars.contains("TM_DIRECTORY")) allVars["TM_DIRECTORY"] = ".";
    if (!allVars.contains("TM_FILEPATH")) allVars["TM_FILEPATH"] = "./untitled";
    if (!allVars.contains("CLIPBOARD")) allVars["CLIPBOARD"] = "";
    if (!allVars.contains("WORKSPACE_NAME")) allVars["WORKSPACE_NAME"] = "workspace";
    
    // Date/time variables
    QDateTime now = QDateTime::currentDateTime();
    allVars["CURRENT_YEAR"] = now.toString("yyyy");
    allVars["CURRENT_YEAR_SHORT"] = now.toString("yy");
    allVars["CURRENT_MONTH"] = now.toString("MM");
    allVars["CURRENT_MONTH_NAME"] = now.toString("MMMM");
    allVars["CURRENT_MONTH_NAME_SHORT"] = now.toString("MMM");
    allVars["CURRENT_DATE"] = now.toString("dd");
    allVars["CURRENT_DAY_NAME"] = now.toString("dddd");
    allVars["CURRENT_DAY_NAME_SHORT"] = now.toString("ddd");
    allVars["CURRENT_HOUR"] = now.toString("HH");
    allVars["CURRENT_MINUTE"] = now.toString("mm");
    allVars["CURRENT_SECOND"] = now.toString("ss");
    
    // Replace ${VAR} and $VAR patterns
    for (auto it = allVars.begin(); it != allVars.end(); ++it) {
        result.replace(QString("${%1}").arg(it.key()), it.value());
        result.replace(QString("$%1").arg(it.key()), it.value());
    }
    
    // Handle ${VAR:default} patterns
    static QRegularExpression varWithDefault(R"(\$\{(\w+):([^}]*)\})");
    QRegularExpressionMatchIterator it = varWithDefault.globalMatch(result);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString varName = match.captured(1);
        QString defaultVal = match.captured(2);
        QString replacement = allVars.value(varName, defaultVal);
        result.replace(match.captured(0), replacement);
    }
    
    return result;
}

QVector<int> Snippet::getTabStops() const {
    QVector<int> stops;
    
    // Match $1, ${1}, ${1:placeholder}
    static QRegularExpression tabStopRe(R"(\$(\d+)|\$\{(\d+)(?::[^}]*)?\})");
    QRegularExpressionMatchIterator it = tabStopRe.globalMatch(body);
    
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        int num = match.captured(1).isEmpty() ? match.captured(2).toInt() : match.captured(1).toInt();
        if (!stops.contains(num)) {
            stops.append(num);
        }
    }
    
    std::sort(stops.begin(), stops.end());
    return stops;
}

QString Snippet::getPlaceholder(int tabStop) const {
    // Match ${N:placeholder}
    QRegularExpression placeholderRe(QString(R"(\$\{%1:([^}]*)\})").arg(tabStop));
    QRegularExpressionMatch match = placeholderRe.match(body);
    
    if (match.hasMatch()) {
        return match.captured(1);
    }
    return QString();
}

// =============================================================================
// SnippetManagerWidget Implementation
// =============================================================================

SnippetManagerWidget::SnippetManagerWidget(QWidget* parent)
    : QWidget(parent)
    , m_settings(new QSettings("RawrXD", "IDE", this))
{
    RawrXD::Integration::ScopedInitTimer init("SnippetManagerWidget");
    setupUI();
    loadSnippets();
    
    // Load built-ins if no snippets exist
    if (m_snippets.isEmpty()) {
        populateBuiltIns();
    }
    
    refreshView();
}

SnippetManagerWidget::~SnippetManagerWidget() {
    saveSnippets();
}

void SnippetManagerWidget::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    
    // Filter bar
    QHBoxLayout* filterLayout = new QHBoxLayout();
    
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Search snippets...");
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &SnippetManagerWidget::onSearchChanged);
    filterLayout->addWidget(m_searchEdit, 2);
    
    filterLayout->addWidget(new QLabel("Language:"));
    m_languageFilter = new QComboBox(this);
    m_languageFilter->addItem("All Languages", "");
    m_languageFilter->setMinimumWidth(100);
    connect(m_languageFilter, &QComboBox::currentTextChanged, this, &SnippetManagerWidget::onLanguageFilterChanged);
    filterLayout->addWidget(m_languageFilter);
    
    filterLayout->addWidget(new QLabel("Category:"));
    m_categoryFilter = new QComboBox(this);
    m_categoryFilter->addItem("All Categories", "");
    m_categoryFilter->setMinimumWidth(100);
    connect(m_categoryFilter, &QComboBox::currentTextChanged, this, &SnippetManagerWidget::onCategoryFilterChanged);
    filterLayout->addWidget(m_categoryFilter);
    
    mainLayout->addLayout(filterLayout);
    
    // Main splitter
    m_splitter = new QSplitter(Qt::Vertical, this);
    
    // Snippet tree
    m_snippetTree = new QTreeWidget(this);
    m_snippetTree->setHeaderLabels({"Name", "Prefix", "Description", "Scope"});
    m_snippetTree->setColumnWidth(0, 200);
    m_snippetTree->setColumnWidth(1, 100);
    m_snippetTree->setColumnWidth(2, 250);
    m_snippetTree->setRootIsDecorated(true);
    m_snippetTree->setAlternatingRowColors(true);
    m_snippetTree->setSortingEnabled(true);
    m_snippetTree->setContextMenuPolicy(Qt::CustomContextMenu);
    
    connect(m_snippetTree, &QTreeWidget::currentItemChanged,
            this, [this](QTreeWidgetItem* item, QTreeWidgetItem*) {
        if (item) onSnippetSelected(item, 0);
    });
    connect(m_snippetTree, &QTreeWidget::itemDoubleClicked,
            this, &SnippetManagerWidget::onSnippetDoubleClicked);
    connect(m_snippetTree, &QTreeWidget::customContextMenuRequested,
            this, [this](const QPoint& pos) {
        QMenu menu(this);
        QTreeWidgetItem* item = m_snippetTree->itemAt(pos);
        
        if (item && !item->data(0, Qt::UserRole).toString().isEmpty()) {
            QAction* insertAction = menu.addAction("Insert Snippet");
            connect(insertAction, &QAction::triggered, this, [this, item]() {
                onSnippetDoubleClicked(item, 0);
            });
            
            QAction* editAction = menu.addAction("Edit Snippet");
            connect(editAction, &QAction::triggered, this, &SnippetManagerWidget::onEditSnippet);
            
            QAction* deleteAction = menu.addAction("Delete Snippet");
            connect(deleteAction, &QAction::triggered, this, &SnippetManagerWidget::onDeleteSnippet);
            
            menu.addSeparator();
        }
        
        QAction* newAction = menu.addAction("New Snippet...");
        connect(newAction, &QAction::triggered, this, &SnippetManagerWidget::onNewSnippet);
        
        QAction* importAction = menu.addAction("Import Snippets...");
        connect(importAction, &QAction::triggered, this, &SnippetManagerWidget::onImportSnippets);
        
        menu.exec(m_snippetTree->mapToGlobal(pos));
    });
    
    m_splitter->addWidget(m_snippetTree);
    
    // Preview panel
    QWidget* previewWidget = new QWidget(this);
    QVBoxLayout* previewLayout = new QVBoxLayout(previewWidget);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    previewLayout->addWidget(new QLabel("Preview:"));
    
    m_previewEdit = new QTextEdit(this);
    m_previewEdit->setReadOnly(true);
    m_previewEdit->setFont(QFont("Consolas", 10));
    m_previewEdit->setStyleSheet("QTextEdit { background-color: #1e1e1e; color: #d4d4d4; }");
    previewLayout->addWidget(m_previewEdit);
    
    m_splitter->addWidget(previewWidget);
    m_splitter->setStretchFactor(0, 2);
    m_splitter->setStretchFactor(1, 1);
    
    mainLayout->addWidget(m_splitter);
    
    // Button bar
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    m_newBtn = new QPushButton("New", this);
    connect(m_newBtn, &QPushButton::clicked, this, &SnippetManagerWidget::onNewSnippet);
    buttonLayout->addWidget(m_newBtn);
    
    m_editBtn = new QPushButton("Edit", this);
    m_editBtn->setEnabled(false);
    connect(m_editBtn, &QPushButton::clicked, this, &SnippetManagerWidget::onEditSnippet);
    buttonLayout->addWidget(m_editBtn);
    
    m_deleteBtn = new QPushButton("Delete", this);
    m_deleteBtn->setEnabled(false);
    connect(m_deleteBtn, &QPushButton::clicked, this, &SnippetManagerWidget::onDeleteSnippet);
    buttonLayout->addWidget(m_deleteBtn);
    
    buttonLayout->addStretch();
    
    m_importBtn = new QPushButton("Import...", this);
    connect(m_importBtn, &QPushButton::clicked, this, &SnippetManagerWidget::onImportSnippets);
    buttonLayout->addWidget(m_importBtn);
    
    m_exportBtn = new QPushButton("Export...", this);
    connect(m_exportBtn, &QPushButton::clicked, this, &SnippetManagerWidget::onExportSnippets);
    buttonLayout->addWidget(m_exportBtn);
    
    mainLayout->addLayout(buttonLayout);
}

void SnippetManagerWidget::loadSnippets() {
    RawrXD::Integration::ScopedTimer timer("SnippetManagerWidget", "loadSnippets", "io");
    m_snippets.clear();
    m_snippetsByLanguage.clear();
    m_snippetsByCategory.clear();
    
    // Load from settings
    int count = m_settings->beginReadArray("Snippets");
    for (int i = 0; i < count; ++i) {
        m_settings->setArrayIndex(i);
        QJsonObject obj = QJsonDocument::fromJson(
            m_settings->value("snippet").toByteArray()).object();
        if (!obj.isEmpty()) {
            m_snippets.append(Snippet::fromJson(obj));
        }
    }
    m_settings->endArray();
    
    // Build indexes
    for (Snippet& s : m_snippets) {
        m_snippetsByLanguage[s.scope].append(&s);
        m_snippetsByCategory[s.category].append(&s);
    }
    
    // Update filter combos
    QSet<QString> languages, categories;
    for (const Snippet& s : m_snippets) {
        if (!s.scope.isEmpty()) languages.insert(s.scope);
        if (!s.category.isEmpty()) categories.insert(s.category);
    }
    
    m_languageFilter->clear();
    m_languageFilter->addItem("All Languages", "");
    for (const QString& lang : languages) {
        m_languageFilter->addItem(lang, lang);
    }
    
    m_categoryFilter->clear();
    m_categoryFilter->addItem("All Categories", "");
    for (const QString& cat : categories) {
        m_categoryFilter->addItem(cat, cat);
    }
}

void SnippetManagerWidget::saveSnippets() {
    RawrXD::Integration::ScopedTimer timer("SnippetManagerWidget", "saveSnippets", "io");
    m_settings->beginWriteArray("Snippets");
    for (int i = 0; i < m_snippets.size(); ++i) {
        m_settings->setArrayIndex(i);
        QJsonDocument doc(m_snippets[i].toJson());
        m_settings->setValue("snippet", doc.toJson(QJsonDocument::Compact));
    }
    m_settings->endArray();
}

void SnippetManagerWidget::populateBuiltIns() {
    // C++ snippets
    auto addCppSnippet = [this](const QString& name, const QString& prefix, 
                                 const QString& desc, const QString& body) {
        Snippet s;
        s.name = name;
        s.prefix = prefix;
        s.description = desc;
        s.body = body;
        s.scope = "cpp";
        s.category = "C++";
        s.isBuiltIn = true;
        m_snippets.append(s);
    };
    
    addCppSnippet("Class Definition", "class", "Create a class",
        "class ${1:ClassName} {\npublic:\n    ${1:ClassName}();\n    ~${1:ClassName}();\n\nprivate:\n    $0\n};");
    
    addCppSnippet("For Loop", "for", "For loop",
        "for (${1:int} ${2:i} = ${3:0}; ${2:i} < ${4:count}; ++${2:i}) {\n    $0\n}");
    
    addCppSnippet("Range-based For", "forr", "Range-based for loop",
        "for (${1:auto}& ${2:item} : ${3:container}) {\n    $0\n}");
    
    addCppSnippet("If Statement", "if", "If statement",
        "if (${1:condition}) {\n    $0\n}");
    
    addCppSnippet("If-Else", "ife", "If-else statement",
        "if (${1:condition}) {\n    $2\n} else {\n    $0\n}");
    
    addCppSnippet("Switch", "switch", "Switch statement",
        "switch (${1:expression}) {\n    case ${2:value}:\n        $0\n        break;\n    default:\n        break;\n}");
    
    addCppSnippet("Struct", "struct", "Struct definition",
        "struct ${1:Name} {\n    $0\n};");
    
    addCppSnippet("Enum Class", "enumc", "Enum class definition",
        "enum class ${1:Name} {\n    $0\n};");
    
    addCppSnippet("Lambda", "lambda", "Lambda expression",
        "[${1:capture}](${2:params}) ${3:-> ReturnType }{\n    $0\n}");
    
    addCppSnippet("Unique Ptr", "uptr", "std::unique_ptr",
        "std::unique_ptr<${1:Type}> ${2:name} = std::make_unique<${1:Type}>(${3:args});");
    
    addCppSnippet("Shared Ptr", "sptr", "std::shared_ptr",
        "std::shared_ptr<${1:Type}> ${2:name} = std::make_shared<${1:Type}>(${3:args});");
    
    addCppSnippet("Try-Catch", "try", "Try-catch block",
        "try {\n    $1\n} catch (const ${2:std::exception}& ${3:e}) {\n    $0\n}");
    
    addCppSnippet("Header Guard", "guard", "Header guard",
        "#ifndef ${1:HEADER}_H\n#define ${1:HEADER}_H\n\n$0\n\n#endif // ${1:HEADER}_H");
    
    addCppSnippet("Pragma Once", "once", "Pragma once",
        "#pragma once\n\n$0");
    
    addCppSnippet("Main Function", "main", "Main function",
        "int main(int argc, char* argv[]) {\n    $0\n    return 0;\n}");
    
    // Python snippets
    auto addPySnippet = [this](const QString& name, const QString& prefix, 
                                const QString& desc, const QString& body) {
        Snippet s;
        s.name = name;
        s.prefix = prefix;
        s.description = desc;
        s.body = body;
        s.scope = "python";
        s.category = "Python";
        s.isBuiltIn = true;
        m_snippets.append(s);
    };
    
    addPySnippet("Function", "def", "Function definition",
        "def ${1:name}(${2:params}):\n    \"\"\"${3:Docstring}\"\"\"\n    $0");
    
    addPySnippet("Class", "class", "Class definition",
        "class ${1:ClassName}:\n    \"\"\"${2:Docstring}\"\"\"\n    \n    def __init__(self${3:, params}):\n        $0");
    
    addPySnippet("If Main", "ifmain", "If __name__ == '__main__'",
        "if __name__ == '__main__':\n    $0");
    
    addPySnippet("For Loop", "for", "For loop",
        "for ${1:item} in ${2:iterable}:\n    $0");
    
    addPySnippet("List Comprehension", "lc", "List comprehension",
        "[${1:expr} for ${2:item} in ${3:iterable}${4: if ${5:condition}}]");
    
    addPySnippet("Dict Comprehension", "dc", "Dict comprehension",
        "{${1:key}: ${2:value} for ${3:item} in ${4:iterable}}");
    
    addPySnippet("Try-Except", "try", "Try-except block",
        "try:\n    $1\nexcept ${2:Exception} as ${3:e}:\n    $0");
    
    addPySnippet("With Statement", "with", "With statement",
        "with ${1:expression} as ${2:var}:\n    $0");
    
    addPySnippet("Lambda", "lambda", "Lambda function",
        "lambda ${1:params}: ${0:expression}");
    
    addPySnippet("Async Function", "adef", "Async function",
        "async def ${1:name}(${2:params}):\n    $0");
    
    // JavaScript/TypeScript snippets
    auto addJsSnippet = [this](const QString& name, const QString& prefix, 
                                const QString& desc, const QString& body) {
        Snippet s;
        s.name = name;
        s.prefix = prefix;
        s.description = desc;
        s.body = body;
        s.scope = "javascript,typescript";
        s.category = "JavaScript";
        s.isBuiltIn = true;
        m_snippets.append(s);
    };
    
    addJsSnippet("Function", "fn", "Function declaration",
        "function ${1:name}(${2:params}) {\n    $0\n}");
    
    addJsSnippet("Arrow Function", "af", "Arrow function",
        "const ${1:name} = (${2:params}) => {\n    $0\n};");
    
    addJsSnippet("Arrow Function Inline", "afi", "Inline arrow function",
        "(${1:params}) => ${0:expression}");
    
    addJsSnippet("Class", "class", "ES6 class",
        "class ${1:ClassName} {\n    constructor(${2:params}) {\n        $0\n    }\n}");
    
    addJsSnippet("For Of", "forof", "For...of loop",
        "for (const ${1:item} of ${2:iterable}) {\n    $0\n}");
    
    addJsSnippet("For In", "forin", "For...in loop",
        "for (const ${1:key} in ${2:object}) {\n    $0\n}");
    
    addJsSnippet("Async Function", "afn", "Async function",
        "async function ${1:name}(${2:params}) {\n    $0\n}");
    
    addJsSnippet("Try-Catch", "try", "Try-catch block",
        "try {\n    $1\n} catch (${2:error}) {\n    $0\n}");
    
    addJsSnippet("Promise", "prom", "New Promise",
        "new Promise((resolve, reject) => {\n    $0\n})");
    
    addJsSnippet("Console Log", "cl", "Console.log",
        "console.log(${1:message});");
    
    addJsSnippet("Import", "imp", "ES6 import",
        "import ${1:module} from '${2:path}';");
    
    addJsSnippet("Export Default", "expd", "Export default",
        "export default ${1:name};");
    
    // Rust snippets
    auto addRustSnippet = [this](const QString& name, const QString& prefix, 
                                  const QString& desc, const QString& body) {
        Snippet s;
        s.name = name;
        s.prefix = prefix;
        s.description = desc;
        s.body = body;
        s.scope = "rust";
        s.category = "Rust";
        s.isBuiltIn = true;
        m_snippets.append(s);
    };
    
    addRustSnippet("Function", "fn", "Function definition",
        "fn ${1:name}(${2:params}) ${3:-> ${4:ReturnType} }{\n    $0\n}");
    
    addRustSnippet("Struct", "struct", "Struct definition",
        "struct ${1:Name} {\n    $0\n}");
    
    addRustSnippet("Impl", "impl", "Implementation block",
        "impl ${1:Type} {\n    $0\n}");
    
    addRustSnippet("Enum", "enum", "Enum definition",
        "enum ${1:Name} {\n    $0\n}");
    
    addRustSnippet("Match", "match", "Match expression",
        "match ${1:expr} {\n    ${2:pattern} => $0,\n}");
    
    addRustSnippet("Main", "main", "Main function",
        "fn main() {\n    $0\n}");
    
    addRustSnippet("Test", "test", "Test function",
        "#[test]\nfn ${1:test_name}() {\n    $0\n}");
    
    saveSnippets();
    loadSnippets();
}

void SnippetManagerWidget::refreshView() {
    m_snippetTree->clear();
    
    QString searchText = m_searchEdit->text().toLower();
    QString langFilter = m_languageFilter->currentData().toString();
    QString catFilter = m_categoryFilter->currentData().toString();
    
    // Group by category
    QMap<QString, QTreeWidgetItem*> categoryItems;
    
    for (const Snippet& s : m_snippets) {
        // Apply filters
        if (!langFilter.isEmpty() && !s.scope.contains(langFilter, Qt::CaseInsensitive)) {
            continue;
        }
        if (!catFilter.isEmpty() && s.category != catFilter) {
            continue;
        }
        if (!searchText.isEmpty()) {
            bool matches = s.name.toLower().contains(searchText) ||
                          s.prefix.toLower().contains(searchText) ||
                          s.description.toLower().contains(searchText);
            for (const QString& kw : s.keywords) {
                if (kw.toLower().contains(searchText)) matches = true;
            }
            if (!matches) continue;
        }
        
        // Get or create category item
        QString cat = s.category.isEmpty() ? "Uncategorized" : s.category;
        if (!categoryItems.contains(cat)) {
            QTreeWidgetItem* catItem = new QTreeWidgetItem(m_snippetTree);
            catItem->setText(0, cat);
            catItem->setExpanded(true);
            catItem->setFlags(catItem->flags() & ~Qt::ItemIsSelectable);
            categoryItems[cat] = catItem;
        }
        
        QTreeWidgetItem* item = new QTreeWidgetItem(categoryItems[cat]);
        item->setText(0, s.name);
        item->setText(1, s.prefix);
        item->setText(2, s.description);
        item->setText(3, s.scope);
        item->setData(0, Qt::UserRole, s.name);
        
        if (s.isBuiltIn) {
            item->setForeground(0, QColor("#9cdcfe"));
        }
        if (!s.isEnabled) {
            item->setForeground(0, Qt::gray);
        }
    }
}

void SnippetManagerWidget::updatePreview(const Snippet& snippet) {
    m_previewEdit->setPlainText(formatSnippetPreview(snippet));
}

QString SnippetManagerWidget::formatSnippetPreview(const Snippet& snippet) const {
    QString preview;
    preview += "Name: " + snippet.name + "\n";
    preview += "Prefix: " + snippet.prefix + "\n";
    preview += "Scope: " + snippet.scope + "\n";
    preview += "Description: " + snippet.description + "\n";
    preview += "\n--- Body ---\n\n";
    preview += snippet.body;
    preview += "\n\n--- Expanded ---\n\n";
    preview += snippet.expandBody();
    return preview;
}

// =============================================================================
// Snippet Access
// =============================================================================

QVector<Snippet> SnippetManagerWidget::getSnippets(const QString& language) const {
    if (language.isEmpty()) {
        return m_snippets;
    }
    
    QVector<Snippet> result;
    for (const Snippet& s : m_snippets) {
        if (s.scope.isEmpty() || s.scope.contains(language, Qt::CaseInsensitive)) {
            result.append(s);
        }
    }
    return result;
}

QVector<Snippet> SnippetManagerWidget::searchSnippets(const QString& query, const QString& language) const {
    QVector<Snippet> result;
    QString lowerQuery = query.toLower();
    
    for (const Snippet& s : m_snippets) {
        if (!language.isEmpty() && !s.scope.isEmpty() && 
            !s.scope.contains(language, Qt::CaseInsensitive)) {
            continue;
        }
        
        bool matches = s.prefix.startsWith(query, Qt::CaseInsensitive) ||
                      s.name.toLower().contains(lowerQuery) ||
                      s.description.toLower().contains(lowerQuery);
        
        if (matches && s.isEnabled) {
            result.append(s);
        }
    }
    
    // Sort by prefix match quality
    std::sort(result.begin(), result.end(), [&query](const Snippet& a, const Snippet& b) {
        bool aExact = a.prefix.compare(query, Qt::CaseInsensitive) == 0;
        bool bExact = b.prefix.compare(query, Qt::CaseInsensitive) == 0;
        if (aExact != bExact) return aExact;
        
        bool aPrefix = a.prefix.startsWith(query, Qt::CaseInsensitive);
        bool bPrefix = b.prefix.startsWith(query, Qt::CaseInsensitive);
        if (aPrefix != bPrefix) return aPrefix;
        
        return a.prefix.length() < b.prefix.length();
    });
    
    return result;
}

Snippet* SnippetManagerWidget::findSnippet(const QString& prefix, const QString& language) {
    for (Snippet& s : m_snippets) {
        if (s.prefix == prefix && s.isEnabled) {
            if (language.isEmpty() || s.scope.isEmpty() || 
                s.scope.contains(language, Qt::CaseInsensitive)) {
                return &s;
            }
        }
    }
    return nullptr;
}

Snippet* SnippetManagerWidget::findSnippetByName(const QString& name) {
    for (Snippet& s : m_snippets) {
        if (s.name == name) {
            return &s;
        }
    }
    return nullptr;
}

// =============================================================================
// Snippet Management
// =============================================================================

void SnippetManagerWidget::addSnippet(const Snippet& snippet) {
    m_snippets.append(snippet);
    saveSnippets();
    loadSnippets();  // Rebuild indexes
    refreshView();
    emit snippetsChanged();
}

void SnippetManagerWidget::updateSnippet(const QString& name, const Snippet& snippet) {
    for (int i = 0; i < m_snippets.size(); ++i) {
        if (m_snippets[i].name == name) {
            m_snippets[i] = snippet;
            saveSnippets();
            loadSnippets();
            refreshView();
            emit snippetsChanged();
            return;
        }
    }
}

void SnippetManagerWidget::removeSnippet(const QString& name) {
    for (int i = 0; i < m_snippets.size(); ++i) {
        if (m_snippets[i].name == name) {
            m_snippets.removeAt(i);
            saveSnippets();
            loadSnippets();
            refreshView();
            emit snippetsChanged();
            return;
        }
    }
}

void SnippetManagerWidget::enableSnippet(const QString& name, bool enabled) {
    for (Snippet& s : m_snippets) {
        if (s.name == name) {
            s.isEnabled = enabled;
            saveSnippets();
            refreshView();
            emit snippetsChanged();
            return;
        }
    }
}

// =============================================================================
// Import/Export
// =============================================================================

void SnippetManagerWidget::importVSCodeSnippets(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Import Failed", 
            QString("Could not open file: %1").arg(path));
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (!doc.isObject()) {
        QMessageBox::warning(this, "Import Failed", "Invalid snippet file format");
        return;
    }
    
    QJsonObject root = doc.object();
    int imported = 0;
    
    for (auto it = root.begin(); it != root.end(); ++it) {
        if (it.value().isObject()) {
            Snippet s = Snippet::fromVSCodeFormat(it.key(), it.value().toObject());
            if (!s.name.isEmpty()) {
                m_snippets.append(s);
                imported++;
            }
        }
    }
    
    saveSnippets();
    loadSnippets();
    refreshView();
    
    QMessageBox::information(this, "Import Complete", 
        QString("Imported %1 snippets").arg(imported));
    emit snippetsChanged();
}

void SnippetManagerWidget::exportSnippets(const QString& path) {
    QJsonObject root;
    
    for (const Snippet& s : m_snippets) {
        QJsonObject snippetObj;
        snippetObj["prefix"] = s.prefix;
        snippetObj["description"] = s.description;
        snippetObj["scope"] = s.scope;
        
        // Split body into lines for VSCode format
        QJsonArray bodyArray;
        for (const QString& line : s.body.split('\n')) {
            bodyArray.append(line);
        }
        snippetObj["body"] = bodyArray;
        
        root[s.name] = snippetObj;
    }
    
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Export Failed", 
            QString("Could not write to file: %1").arg(path));
        return;
    }
    
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    
    QMessageBox::information(this, "Export Complete", 
        QString("Exported %1 snippets").arg(m_snippets.size()));
}

void SnippetManagerWidget::loadBuiltInSnippets() {
    populateBuiltIns();
}

QStringList SnippetManagerWidget::getCategories() const {
    QSet<QString> cats;
    for (const Snippet& s : m_snippets) {
        if (!s.category.isEmpty()) {
            cats.insert(s.category);
        }
    }
    return cats.values();
}

QStringList SnippetManagerWidget::getLanguages() const {
    QSet<QString> langs;
    for (const Snippet& s : m_snippets) {
        for (const QString& scope : s.scope.split(',', Qt::SkipEmptyParts)) {
            langs.insert(scope.trimmed());
        }
    }
    return langs.values();
}

// =============================================================================
// Slots
// =============================================================================

void SnippetManagerWidget::onSearchChanged(const QString& text) {
    Q_UNUSED(text)
    refreshView();
}

void SnippetManagerWidget::onLanguageFilterChanged(const QString& language) {
    Q_UNUSED(language)
    refreshView();
}

void SnippetManagerWidget::onCategoryFilterChanged(const QString& category) {
    Q_UNUSED(category)
    refreshView();
}

void SnippetManagerWidget::onSnippetSelected(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column)
    
    QString name = item->data(0, Qt::UserRole).toString();
    if (name.isEmpty()) {
        m_currentSnippet = nullptr;
        m_previewEdit->clear();
        m_editBtn->setEnabled(false);
        m_deleteBtn->setEnabled(false);
        return;
    }
    
    m_currentSnippet = findSnippetByName(name);
    if (m_currentSnippet) {
        updatePreview(*m_currentSnippet);
        m_editBtn->setEnabled(true);
        m_deleteBtn->setEnabled(!m_currentSnippet->isBuiltIn);
        emit snippetSelected(*m_currentSnippet);
    }
}

void SnippetManagerWidget::onSnippetDoubleClicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column)
    
    QString name = item->data(0, Qt::UserRole).toString();
    if (name.isEmpty()) return;
    
    Snippet* s = findSnippetByName(name);
    if (s) {
        QString expanded = s->expandBody();
        emit snippetInsertRequested(expanded);
    }
}

void SnippetManagerWidget::onNewSnippet() {
    SnippetEditDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        addSnippet(dialog.getSnippet());
    }
}

void SnippetManagerWidget::onEditSnippet() {
    if (!m_currentSnippet) return;
    
    SnippetEditDialog dialog(this);
    dialog.setSnippet(*m_currentSnippet);
    
    if (dialog.exec() == QDialog::Accepted) {
        updateSnippet(m_currentSnippet->name, dialog.getSnippet());
    }
}

void SnippetManagerWidget::onDeleteSnippet() {
    if (!m_currentSnippet) return;
    
    if (m_currentSnippet->isBuiltIn) {
        QMessageBox::warning(this, "Cannot Delete", "Built-in snippets cannot be deleted.");
        return;
    }
    
    if (QMessageBox::question(this, "Delete Snippet",
            QString("Delete snippet '%1'?").arg(m_currentSnippet->name),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        removeSnippet(m_currentSnippet->name);
        m_currentSnippet = nullptr;
    }
}

void SnippetManagerWidget::onImportSnippets() {
    QString path = QFileDialog::getOpenFileName(this, "Import Snippets",
        QString(), "JSON Files (*.json);;All Files (*)");
    if (!path.isEmpty()) {
        importVSCodeSnippets(path);
    }
}

void SnippetManagerWidget::onExportSnippets() {
    QString path = QFileDialog::getSaveFileName(this, "Export Snippets",
        "snippets.json", "JSON Files (*.json)");
    if (!path.isEmpty()) {
        exportSnippets(path);
    }
}

// =============================================================================
// SnippetEditDialog Implementation
// =============================================================================

SnippetEditDialog::SnippetEditDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUI();
    setWindowTitle("Edit Snippet");
    resize(600, 500);
}

void SnippetEditDialog::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    QFormLayout* formLayout = new QFormLayout();
    
    m_nameEdit = new QLineEdit(this);
    formLayout->addRow("Name:", m_nameEdit);
    
    m_prefixEdit = new QLineEdit(this);
    m_prefixEdit->setPlaceholderText("Trigger text");
    formLayout->addRow("Prefix:", m_prefixEdit);
    
    m_descEdit = new QLineEdit(this);
    formLayout->addRow("Description:", m_descEdit);
    
    m_scopeCombo = new QComboBox(this);
    m_scopeCombo->setEditable(true);
    m_scopeCombo->addItems({"", "cpp", "python", "javascript", "typescript", 
                           "rust", "java", "csharp", "go", "html", "css"});
    formLayout->addRow("Scope:", m_scopeCombo);
    
    m_categoryCombo = new QComboBox(this);
    m_categoryCombo->setEditable(true);
    m_categoryCombo->addItems({"", "C++", "Python", "JavaScript", "Rust", 
                               "Control Flow", "Classes", "Functions", "Comments"});
    formLayout->addRow("Category:", m_categoryCombo);
    
    m_keywordsEdit = new QLineEdit(this);
    m_keywordsEdit->setPlaceholderText("Comma-separated keywords");
    formLayout->addRow("Keywords:", m_keywordsEdit);
    
    mainLayout->addLayout(formLayout);
    
    // Body editor
    QGroupBox* bodyGroup = new QGroupBox("Snippet Body", this);
    QVBoxLayout* bodyLayout = new QVBoxLayout(bodyGroup);
    
    QLabel* helpLabel = new QLabel("Use $1, $2, etc. for tab stops. "
        "Use ${1:placeholder} for placeholders. $0 marks final cursor position.", this);
    helpLabel->setWordWrap(true);
    helpLabel->setStyleSheet("color: gray; font-size: 10px;");
    bodyLayout->addWidget(helpLabel);
    
    m_bodyEdit = new QTextEdit(this);
    m_bodyEdit->setFont(QFont("Consolas", 10));
    m_bodyEdit->setMinimumHeight(150);
    bodyLayout->addWidget(m_bodyEdit);
    
    mainLayout->addWidget(bodyGroup);
    
    // Preview
    QGroupBox* previewGroup = new QGroupBox("Preview", this);
    QVBoxLayout* previewLayout = new QVBoxLayout(previewGroup);
    
    m_previewEdit = new QTextEdit(this);
    m_previewEdit->setReadOnly(true);
    m_previewEdit->setFont(QFont("Consolas", 10));
    m_previewEdit->setMaximumHeight(100);
    previewLayout->addWidget(m_previewEdit);
    
    QPushButton* previewBtn = new QPushButton("Update Preview", this);
    connect(previewBtn, &QPushButton::clicked, this, &SnippetEditDialog::onPreviewBody);
    previewLayout->addWidget(previewBtn);
    
    mainLayout->addWidget(previewGroup);
    
    // Buttons
    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okBtn = buttons->button(QDialogButtonBox::Ok);
    m_okBtn->setEnabled(false);
    
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    
    mainLayout->addWidget(buttons);
    
    // Validation
    connect(m_nameEdit, &QLineEdit::textChanged, this, &SnippetEditDialog::validateInput);
    connect(m_prefixEdit, &QLineEdit::textChanged, this, &SnippetEditDialog::validateInput);
    connect(m_bodyEdit, &QTextEdit::textChanged, this, &SnippetEditDialog::validateInput);
}

void SnippetEditDialog::setSnippet(const Snippet& snippet) {
    m_nameEdit->setText(snippet.name);
    m_prefixEdit->setText(snippet.prefix);
    m_descEdit->setText(snippet.description);
    m_bodyEdit->setPlainText(snippet.body);
    m_scopeCombo->setCurrentText(snippet.scope);
    m_categoryCombo->setCurrentText(snippet.category);
    m_keywordsEdit->setText(snippet.keywords.join(", "));
    
    onPreviewBody();
    validateInput();
}

Snippet SnippetEditDialog::getSnippet() const {
    Snippet s;
    s.name = m_nameEdit->text().trimmed();
    s.prefix = m_prefixEdit->text().trimmed();
    s.description = m_descEdit->text().trimmed();
    s.body = m_bodyEdit->toPlainText();
    s.scope = m_scopeCombo->currentText().trimmed();
    s.category = m_categoryCombo->currentText().trimmed();
    
    QString keywords = m_keywordsEdit->text().trimmed();
    if (!keywords.isEmpty()) {
        s.keywords = keywords.split(',', Qt::SkipEmptyParts);
        for (QString& kw : s.keywords) {
            kw = kw.trimmed();
        }
    }
    
    return s;
}

void SnippetEditDialog::onPreviewBody() {
    Snippet temp = getSnippet();
    m_previewEdit->setPlainText(temp.expandBody());
}

void SnippetEditDialog::validateInput() {
    bool valid = !m_nameEdit->text().trimmed().isEmpty() &&
                 !m_prefixEdit->text().trimmed().isEmpty() &&
                 !m_bodyEdit->toPlainText().trimmed().isEmpty();
    m_okBtn->setEnabled(valid);
}
