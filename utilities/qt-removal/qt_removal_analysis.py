#!/usr/bin/env python3
"""
Qt Removal Automation Script
Removes all Qt framework dependencies from RawrXD codebase
Replaces with pure C++20/Win32 alternatives
"""

import os
import re
import sys
from pathlib import Path
from collections import defaultdict

# Define all Qt includes to remove
QT_INCLUDES_TO_REMOVE = {
    '#include <QMainWindow>',
    '#include <QWidget>',
    '#include <QDialog>',
    '#include <QApplication>',
    '#include <QCoreApplication>',
    '#include <QString>',
    '#include <QStringList>',
    '#include <QList>',
    '#include <QVector>',
    '#include <QHash>',
    '#include <QMap>',
    '#include <QFile>',
    '#include <QDir>',
    '#include <QFileInfo>',
    '#include <QThread>',
    '#include <QMutex>',
    '#include <QReadWriteLock>',
    '#include <QTimer>',
    '#include <QEvent>',
    '#include <QVariant>',
    '#include <QSettings>',
    '#include <QDateTime>',
    '#include <QDate>',
    '#include <QTime>',
    '#include <QPoint>',
    '#include <QSize>',
    '#include <QRect>',
    '#include <QColor>',
    '#include <QFont>',
    '#include <QIcon>',
    '#include <QPixmap>',
    '#include <QImage>',
    '#include <QPalette>',
    '#include <QBrush>',
    '#include <QPen>',
    '#include <QPainter>',
    '#include <QMessageBox>',
    '#include <QFileDialog>',
    '#include <QInputDialog>',
    '#include <QProgressDialog>',
    '#include <QWizard>',
    '#include <QDockWidget>',
    '#include <QToolBar>',
    '#include <QStatusBar>',
    '#include <QMenuBar>',
    '#include <QMenu>',
    '#include <QAction>',
    '#include <QToolButton>',
    '#include <QPushButton>',
    '#include <QCheckBox>',
    '#include <QRadioButton>',
    '#include <QLineEdit>',
    '#include <QTextEdit>',
    '#include <QPlainTextEdit>',
    '#include <QComboBox>',
    '#include <QSpinBox>',
    '#include <QSlider>',
    '#include <QScrollBar>',
    '#include <QLabel>',
    '#include <QListWidget>',
    '#include <QTreeWidget>',
    '#include <QTableWidget>',
    '#include <QTabWidget>',
    '#include <QStackedWidget>',
    '#include <QScrollArea>',
    '#include <QSplitter>',
    '#include <QLayout>',
    '#include <QVBoxLayout>',
    '#include <QHBoxLayout>',
    '#include <QGridLayout>',
    '#include <QFormLayout>',
    '#include <QNetworkAccessManager>',
    '#include <QNetworkRequest>',
    '#include <QNetworkReply>',
    '#include <QHttpMultiPart>',
    '#include <QUrl>',
    '#include <QUrlQuery>',
    '#include <QJsonObject>',
    '#include <QJsonArray>',
    '#include <QJsonDocument>',
    '#include <QJsonValue>',
    '#include <QXmlStreamReader>',
    '#include <QXmlStreamWriter>',
    '#include <QSql>',
    '#include <QSqlDatabase>',
    '#include <QSqlQuery>',
    '#include <QSqlError>',
    '#include <QSqlRecord>',
    '#include <QProcess>',
    '#include <QLocalServer>',
    '#include <QLocalSocket>',
    '#include <QTcpServer>',
    '#include <QTcpSocket>',
    '#include <QUdpSocket>',
    '#include <QHostAddress>',
    '#include <QHostInfo>',
}

# Qt macros to remove
QT_MACROS_TO_REMOVE = {
    'Q_OBJECT',
    'Q_PROPERTY',
    'Q_ENUM',
    'Q_FLAG',
    'Q_GADGET',
    'Q_DECLARE_METATYPE',
    'Q_DECLARE_INTERFACE',
    'Q_INTERFACES',
    'Q_PLUGIN_METADATA',
    'QT_BEGIN_NAMESPACE',
    'QT_END_NAMESPACE',
    'QT_FORWARD_DECLARE_CLASS',
    'QT_FORWARD_DECLARE_STRUCT',
}

# Qt function patterns to update
QT_FUNCTION_REPLACEMENTS = {
    r'\bqDebug\(\)': 'std::cerr',
    r'\bqWarning\(\)': 'std::cerr',
    r'\bqCritical\(\)': 'std::cerr',
    r'\bqFatal\(': 'throw std::runtime_error(',
    r'\bqrand\(\)': 'rand()',
    r'\bqSleep\(': 'std::this_thread::sleep_for(std::chrono::milliseconds(',
}

def count_qt_usages(src_dir):
    """Count Qt usages across all files"""
    stats = defaultdict(int)
    file_count = 0
    include_count = 0
    
    for root, dirs, files in os.walk(src_dir):
        # Skip build directories
        dirs[:] = [d for d in dirs if d not in ['build', '.git', '.vs', 'cmake', '__pycache__']]
        
        for file in files:
            if not any(file.endswith(ext) for ext in ['.cpp', '.h', '.hpp', '.c']):
                continue
                
            filepath = os.path.join(root, file)
            file_count += 1
            
            try:
                with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                    for line in f:
                        for include in QT_INCLUDES_TO_REMOVE:
                            if include in line:
                                stats['includes'] += 1
                                include_count += 1
                                break
                        
                        for macro in QT_MACROS_TO_REMOVE:
                            if macro in line:
                                stats['macros'] += 1
                                break
            except Exception as e:
                pass
    
    return file_count, stats

def main():
    src_dir = r'D:\RawrXD\src'
    
    print("=" * 80)
    print("Qt Removal Analysis for RawrXD")
    print("=" * 80)
    
    file_count, stats = count_qt_usages(src_dir)
    
    print(f"\nTotal files scanned: {file_count}")
    print(f"Qt includes found: {stats['includes']}")
    print(f"Qt macros found: {stats['macros']}")
    print(f"\nPrimary Qt includes to remove: {len(QT_INCLUDES_TO_REMOVE)}")
    print(f"Primary Qt macros to remove: {len(QT_MACROS_TO_REMOVE)}")
    
    print("\nTop priorities for removal:")
    print("1. qtapp/ folder (heaviest Qt usage)")
    print("2. agent/ folder (action_executor, agent_hot_patcher, auto_update, etc.)")
    print("3. agentic_* files (core agentic engine)")
    print("4. auth/, feedback/, orchestration/ (UI/config systems)")
    print("5. UI subsystems (training, inference, etc.)")
    print("6. Utility and platform code")
    
    print("\nReplacement strategy:")
    print("- Add #include \"QtReplacements.hpp\" to all files")
    print("- Remove all #include <Q*> directives")
    print("- Use std::wstring instead of QString")
    print("- Use std::vector instead of QList/QVector")
    print("- Use Win32 API directly instead of Qt widgets")
    print("- Update CMakeLists.txt to remove Qt dependencies")

if __name__ == '__main__':
    main()
