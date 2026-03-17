#!/usr/bin/env python3
"""
RawrXD IDE Widget Generator
Rapidly scaffolds production-ready widget implementations following the established pattern.

Usage:
    python generate_widget.py WidgetName "Widget Description" [category]
    
Example:
    python generate_widget.py RunDebugWidget "Debugger with GDB/LLDB support" debug
"""

import sys
import os
from datetime import datetime

WIDGET_HEADER_TEMPLATE = '''#pragma once

#include <QWidget>
#include <QString>
#include <QDateTime>

QT_BEGIN_NAMESPACE
// Forward declare Qt classes as needed
class QPushButton;
class QLabel;
class QToolBar;
class QTextEdit;
class QTreeWidget;
class QTabWidget;
QT_END_NAMESPACE

/**
 * @brief Production-ready {WIDGET_NAME}
 * 
 * {DESCRIPTION}
 * 
 * Features:
 * - Feature 1
 * - Feature 2
 * - Feature 3
 * - Structured logging and metrics
 * - Settings persistence
 * - Error handling
 * 
 * @date {DATE}
 * @author RawrXD Team
 */
class {WIDGET_NAME} : public QWidget
{{
    Q_OBJECT

public:
    explicit {WIDGET_NAME}(QWidget* parent = nullptr);
    ~{WIDGET_NAME}() override;

    // Configuration
    void setConfiguration(const QString& config);
    QString configuration() const {{ return m_config; }}
    
    // Core operations
    void initialize();
    void reset();
    
    // Statistics
    struct Statistics {{
        int operationCount = 0;
        QDateTime lastOperationTime;
    }};
    Statistics statistics() const {{ return m_stats; }}

signals:
    void operationCompleted(bool success);
    void stateChanged(const QString& newState);
    void errorOccurred(const QString& error);

public slots:
    void onActionTriggered();
    void onRefresh();

private slots:
    void onInternalEvent();

private:
    void setupUI();
    void createToolBar();
    void createMainView();
    void connectSignals();
    void loadSettings();
    void saveSettings();
    void logEvent(const QString& event, const QJsonObject& data = QJsonObject());
    
    // UI Components
    QToolBar* m_toolBar{{nullptr}};
    QTabWidget* m_tabWidget{{nullptr}};
    QTextEdit* m_outputView{{nullptr}};
    QLabel* m_statusLabel{{nullptr}};
    
    // Data
    QString m_config;
    Statistics m_stats;
    bool m_initialized{{false}};
}};
'''

WIDGET_CPP_TEMPLATE = '''#include "{WIDGET_NAME_LOWER}.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QToolBar>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTabWidget>
#include <QMessageBox>
#include <QSettings>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonDocument>
#include <QDebug>

{WIDGET_NAME}::{WIDGET_NAME}(QWidget* parent)
    : QWidget(parent)
{{
    setupUI();
    connectSignals();
    loadSettings();
    logEvent("widget_initialized");
}}

{WIDGET_NAME}::~{WIDGET_NAME}()
{{
    saveSettings();
    logEvent("widget_destroyed", QJsonObject{{
        {{"operation_count", m_stats.operationCount}}
    }});
}}

void {WIDGET_NAME}::setupUI()
{{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);
    
    // Toolbar
    createToolBar();
    mainLayout->addWidget(m_toolBar);
    
    // Status bar
    QHBoxLayout* statusLayout = new QHBoxLayout();
    m_statusLabel = new QLabel("Ready", this);
    m_statusLabel->setStyleSheet("QLabel {{ padding: 4px; }}");
    statusLayout->addWidget(m_statusLabel, 1);
    mainLayout->addLayout(statusLayout);
    
    // Main content
    createMainView();
    mainLayout->addWidget(m_tabWidget, 1);
}}

void {WIDGET_NAME}::createToolBar()
{{
    m_toolBar = new QToolBar("{WIDGET_NAME} Tools", this);
    m_toolBar->setIconSize(QSize(16, 16));
    
    QPushButton* refreshBtn = new QPushButton(QIcon(":/icons/refresh.png"), "Refresh", this);
    refreshBtn->setToolTip("Refresh (F5)");
    refreshBtn->setShortcut(Qt::Key_F5);
    connect(refreshBtn, &QPushButton::clicked, this, &{WIDGET_NAME}::onRefresh);
    m_toolBar->addWidget(refreshBtn);
    
    m_toolBar->addSeparator();
    
    QPushButton* actionBtn = new QPushButton(QIcon(":/icons/action.png"), "Action", this);
    actionBtn->setToolTip("Perform action");
    connect(actionBtn, &QPushButton::clicked, this, &{WIDGET_NAME}::onActionTriggered);
    m_toolBar->addWidget(actionBtn);
}}

void {WIDGET_NAME}::createMainView()
{{
    m_tabWidget = new QTabWidget(this);
    
    // Main output view
    m_outputView = new QTextEdit(this);
    m_outputView->setReadOnly(true);
    m_outputView->setFont(QFont("Consolas", 9));
    m_outputView->setStyleSheet("QTextEdit {{ background-color: #1e1e1e; color: #d4d4d4; }}");
    m_tabWidget->addTab(m_outputView, QIcon(":/icons/output.png"), "Output");
    
    // Additional tabs as needed
    // m_tabWidget->addTab(createSecondTab(), "Tab 2");
}}

void {WIDGET_NAME}::connectSignals()
{{
    // Connect internal signals/slots
    // connect(m_component, &Class::signal, this, &{WIDGET_NAME}::slot);
}}

void {WIDGET_NAME}::initialize()
{{
    if (m_initialized) {{
        qWarning() << "{WIDGET_NAME}: Already initialized";
        return;
    }}
    
    m_outputView->append("Initializing {WIDGET_NAME}...\\n");
    
    // Perform initialization
    
    m_initialized = true;
    m_statusLabel->setText("Initialized");
    emit stateChanged("initialized");
    logEvent("initialized");
}}

void {WIDGET_NAME}::reset()
{{
    m_outputView->clear();
    m_stats = Statistics{{}};
    m_initialized = false;
    m_statusLabel->setText("Reset");
    emit stateChanged("reset");
    logEvent("reset");
}}

void {WIDGET_NAME}::setConfiguration(const QString& config)
{{
    m_config = config;
    saveSettings();
    logEvent("configuration_changed", QJsonObject{{{{"config", config}}}});
}}

void {WIDGET_NAME}::onActionTriggered()
{{
    if (!m_initialized) {{
        initialize();
    }}
    
    m_outputView->append("Action triggered\\n");
    m_stats.operationCount++;
    m_stats.lastOperationTime = QDateTime::currentDateTime();
    
    emit operationCompleted(true);
    logEvent("action_triggered");
}}

void {WIDGET_NAME}::onRefresh()
{{
    m_outputView->append("Refreshing...\\n");
    
    // Perform refresh operation
    
    m_statusLabel->setText("Refreshed");
    logEvent("refreshed");
}}

void {WIDGET_NAME}::onInternalEvent()
{{
    // Handle internal events
}}

void {WIDGET_NAME}::loadSettings()
{{
    QSettings settings("RawrXD", "IDE");
    settings.beginGroup("{WIDGET_NAME}");
    m_config = settings.value("config", "default").toString();
    settings.endGroup();
}}

void {WIDGET_NAME}::saveSettings()
{{
    QSettings settings("RawrXD", "IDE");
    settings.beginGroup("{WIDGET_NAME}");
    settings.setValue("config", m_config);
    settings.endGroup();
}}

void {WIDGET_NAME}::logEvent(const QString& event, const QJsonObject& data)
{{
    QJsonObject logEntry;
    logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    logEntry["component"] = "{WIDGET_NAME}";
    logEntry["event"] = event;
    logEntry["data"] = data;
    
    qDebug().noquote() << "EVENT:" << QJsonDocument(logEntry).toJson(QJsonDocument::Compact);
}}
'''

CMAKE_ADDITION = '''
# Add to src/qtapp/CMakeLists.txt in the appropriate section:

# Widget sources
widgets/{WIDGET_NAME_LOWER}.cpp
widgets/{WIDGET_NAME_LOWER}.h
'''

MAINWINDOW_SLOT_TEMPLATE = '''
// Add to MainWindow.cpp toggle slot implementations:

void MainWindow::toggle{WIDGET_NAME}(bool visible)
{{
    if (!m_{WIDGET_VAR}) {{
        m_{WIDGET_VAR} = new {WIDGET_NAME}(this);
        
        QDockWidget* dock = new QDockWidget("{WIDGET_TITLE}", this);
        dock->setWidget(m_{WIDGET_VAR});
        dock->setObjectName("{WIDGET_NAME}Dock");
        dock->setAllowedAreas(Qt::AllDockWidgetAreas);
        dock->setFeatures(QDockWidget::DockWidgetMovable |
                         QDockWidget::DockWidgetFloatable |
                         QDockWidget::DockWidgetClosable);
        
        addDockWidget(Qt::RightDockWidgetArea, dock);
        
        // Connect signals
        connect(m_{WIDGET_VAR}, &{WIDGET_NAME}::operationCompleted,
                this, [this](bool success) {{
                    QString msg = success ? "Operation completed" : "Operation failed";
                    statusBar()->showMessage(msg, 3000);
                }});
        
        connect(m_{WIDGET_VAR}, &{WIDGET_NAME}::errorOccurred,
                this, [this](const QString& error) {{
                    QMessageBox::warning(this, "{WIDGET_TITLE} Error", error);
                }});
    }}
    
    if (m_{WIDGET_VAR}) {{
        m_{WIDGET_VAR}->setVisible(visible);
    }}
}}
'''

def to_snake_case(name):
    """Convert CamelCase to snake_case"""
    import re
    name = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', name)
    return re.sub('([a-z0-9])([A-Z])', r'\1_\2', name).lower()

def generate_widget(widget_name, description, category="general"):
    """Generate widget files"""
    
    date = datetime.now().strftime("%Y-%m-%d")
    widget_name_lower = to_snake_case(widget_name)
    widget_var = widget_name[0].lower() + widget_name[1:] if len(widget_name) > 1 else widget_name.lower()
    widget_title = ' '.join([w.capitalize() for w in widget_name.replace('Widget', '').split()])
    
    # Create widgets directory if it doesn't exist
    widgets_dir = "src/qtapp/widgets"
    os.makedirs(widgets_dir, exist_ok=True)
    
    # Generate header
    header_content = WIDGET_HEADER_TEMPLATE.format(
        WIDGET_NAME=widget_name,
        DESCRIPTION=description,
        DATE=date
    )
    
    header_path = os.path.join(widgets_dir, f"{widget_name_lower}.h")
    with open(header_path, 'w') as f:
        f.write(header_content)
    print(f"✅ Generated: {header_path}")
    
    # Generate implementation
    cpp_content = WIDGET_CPP_TEMPLATE.format(
        WIDGET_NAME=widget_name,
        WIDGET_NAME_LOWER=widget_name_lower
    )
    
    cpp_path = os.path.join(widgets_dir, f"{widget_name_lower}.cpp")
    with open(cpp_path, 'w') as f:
        f.write(cpp_content)
    print(f"✅ Generated: {cpp_path}")
    
    # Generate integration instructions
    integration_path = f"INTEGRATION_{widget_name}.md"
    with open(integration_path, 'w') as f:
        f.write(f"# {widget_name} Integration Guide\n\n")
        f.write(f"## Files Created\n\n")
        f.write(f"- `{header_path}`\n")
        f.write(f"- `{cpp_path}`\n\n")
        
        f.write(f"## CMake Integration\n\n")
        f.write(CMAKE_ADDITION.format(WIDGET_NAME_LOWER=widget_name_lower))
        f.write("\n\n")
        
        f.write(f"## MainWindow Integration\n\n")
        f.write(MAINWINDOW_SLOT_TEMPLATE.format(
            WIDGET_NAME=widget_name,
            WIDGET_VAR=widget_var,
            WIDGET_TITLE=widget_title
        ))
        f.write("\n\n")
        
        f.write(f"## Subsystems.h Update\n\n")
        f.write(f"Replace the stub definition with:\n\n")
        f.write(f"```cpp\n")
        f.write(f"#include \"widgets/{widget_name_lower}.h\"\n")
        f.write(f"```\n\n")
        
        f.write(f"## Next Steps\n\n")
        f.write(f"1. Implement specific functionality in {widget_name_lower}.cpp\n")
        f.write(f"2. Add widget to CMakeLists.txt\n")
        f.write(f"3. Update Subsystems.h\n")
        f.write(f"4. Implement toggle slot in MainWindow.cpp\n")
        f.write(f"5. Add unit tests\n")
        f.write(f"6. Update documentation\n")
    
    print(f"✅ Generated: {integration_path}")
    
    print(f"\n🎉 Widget '{widget_name}' scaffolded successfully!")
    print(f"\n📝 Next steps:")
    print(f"   1. Edit {cpp_path} to add specific functionality")
    print(f"   2. Add to CMakeLists.txt")
    print(f"   3. Update Subsystems.h")
    print(f"   4. Implement MainWindow toggle slot")
    print(f"   5. See {integration_path} for details")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    
    widget_name = sys.argv[1]
    description = sys.argv[2]
    category = sys.argv[3] if len(sys.argv) > 3 else "general"
    
    if not widget_name.endswith("Widget"):
        widget_name += "Widget"
    
    generate_widget(widget_name, description, category)
