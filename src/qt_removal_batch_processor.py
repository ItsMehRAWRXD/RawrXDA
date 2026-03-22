#!/usr/bin/env python3
"""
Qt Removal Batch Processor
Automatically removes Qt dependencies from C++ source files
Replaces with pure C++20/Win32 alternatives
NO INSTRUMENTATION OR LOGGING - per requirements
"""

import os
import re
import sys
from pathlib import Path
from collections import defaultdict

class QtRemovalProcessor:
    """Process files to remove Qt dependencies"""
    
    # All Qt includes to remove
    QT_INCLUDES = {
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
        '#include <QSet>',
        '#include <QQueue>',
        '#include <QStack>',
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
    
    # Qt macros to remove/replace
    QT_MACROS = {
        'Q_OBJECT': '',
        'Q_PROPERTY': '',
        'Q_ENUM': '',
        'Q_FLAG': '',
        'Q_GADGET': '',
        'Q_DECLARE_METATYPE': '',
        'Q_INTERFACE': '',
        'QT_BEGIN_NAMESPACE': '',
        'QT_END_NAMESPACE': '',
    }
    
    def __init__(self, src_dir):
        self.src_dir = src_dir
        self.stats = defaultdict(int)
        self.processed_files = []
        self.errors = []
    
    def process_file(self, filepath):
        """Process a single file to remove Qt dependencies"""
        try:
            with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
            
            original_content = content
            
            # Check if file has Qt includes
            has_qt = any(inc in content for inc in self.QT_INCLUDES)
            if not has_qt:
                return False
            
            # 1. Add RawrCompatIo.hpp include (after #pragma once)
            if '#pragma once' in content:
                content = content.replace(
                    '#pragma once',
                    '#pragma once\n#include "RawrCompatIo.hpp"',
                    1
                )
            elif content.startswith('#ifndef'):
                # Header guard style
                lines = content.split('\n', 3)
                if len(lines) >= 3:
                    content = '\n'.join(lines[:3]) + '\n#include "RawrCompatIo.hpp"\n' + '\n'.join(lines[3:])
            else:
                # No header guards - add at very top
                content = '#include "RawrCompatIo.hpp"\n' + content
            
            # 2. Remove all Qt includes
            for qt_inc in self.QT_INCLUDES:
                content = re.sub(re.escape(qt_inc) + r'\s*\n', '', content)
                content = re.sub(re.escape(qt_inc) + r'\s*$', '', content, flags=re.MULTILINE)
            
            # 3. Remove Q_OBJECT and Q_PROPERTY macros
            content = re.sub(r'^\s*Q_OBJECT\s*$', '', content, flags=re.MULTILINE)
            content = re.sub(r'^\s*Q_PROPERTY\([^)]*\)\s*$', '', content, flags=re.MULTILINE)
            content = re.sub(r'^\s*Q_ENUM\([^)]*\)\s*$', '', content, flags=re.MULTILINE)
            content = re.sub(r'^\s*Q_FLAG\([^)]*\)\s*$', '', content, flags=re.MULTILINE)
            content = re.sub(r'^\s*Q_GADGET\s*$', '', content, flags=re.MULTILINE)
            content = re.sub(r'^\s*Q_DECLARE_METATYPE\([^)]*\)\s*$', '', content, flags=re.MULTILINE)
            content = re.sub(r'^\s*Q_INTERFACE\([^)]*\)\s*$', '', content, flags=re.MULTILINE)
            
            # 4. Remove QT_BEGIN_NAMESPACE / QT_END_NAMESPACE
            content = re.sub(r'^\s*QT_BEGIN_NAMESPACE\s*$', '', content, flags=re.MULTILINE)
            content = re.sub(r'^\s*QT_END_NAMESPACE\s*$', '', content, flags=re.MULTILINE)
            
            # 5. Clean up multiple blank lines
            content = re.sub(r'\n\n\n+', '\n\n', content)
            
            # Only write if changed
            if content != original_content:
                with open(filepath, 'w', encoding='utf-8') as f:
                    f.write(content)
                
                self.stats['files_processed'] += 1
                self.processed_files.append(filepath)
                return True
            
            return False
            
        except Exception as e:
            self.errors.append((filepath, str(e)))
            self.stats['errors'] += 1
            return False
    
    def process_batch(self, folder_pattern):
        """Process all files matching folder pattern"""
        processed = 0
        
        for root, dirs, files in os.walk(self.src_dir):
            # Skip build and test directories
            dirs[:] = [d for d in dirs if d not in ['build', '.git', '.vs', 'cmake', '__pycache__']]
            
            # Check if this folder matches pattern
            rel_path = os.path.relpath(root, self.src_dir)
            if not any(pattern in rel_path.lower() for pattern in folder_pattern.split('|')):
                continue
            
            for file in files:
                if not any(file.endswith(ext) for ext in ['.cpp', '.h', '.hpp', '.c']):
                    continue
                
                filepath = os.path.join(root, file)
                if self.process_file(filepath):
                    processed += 1
        
        return processed
    
    def process_all(self):
        """Process all files in src directory"""
        processed = 0
        
        for root, dirs, files in os.walk(self.src_dir):
            dirs[:] = [d for d in dirs if d not in ['build', '.git', '.vs', 'cmake', '__pycache__']]
            
            for file in files:
                if not any(file.endswith(ext) for ext in ['.cpp', '.h', '.hpp', '.c']):
                    continue
                
                filepath = os.path.join(root, file)
                if self.process_file(filepath):
                    processed += 1
        
        return processed
    
    def report(self):
        """Print processing report"""
        print("\n" + "=" * 80)
        print("Qt Removal Batch Processing Report")
        print("=" * 80)
        print(f"\nFiles processed: {self.stats['files_processed']}")
        print(f"Errors: {self.stats['errors']}")
        
        if self.errors:
            print("\nErrors encountered:")
            for filepath, error in self.errors:
                print(f"  {filepath}: {error}")
        
        print("\n" + "=" * 80)


def main():
    src_dir = r'D:\RawrXD\src'
    
    if len(sys.argv) > 1:
        batch = sys.argv[1]
        processor = QtRemovalProcessor(src_dir)
        
        # Map batch names to folder patterns
        batches = {
            '1': 'qtapp',
            '2': 'agent',
            '3': 'agentic',
            '4': 'auth|feedback|setup|orchestration|thermal',
            '5': 'training|inference|streaming',
            'all': '',
        }
        
        if batch in batches:
            pattern = batches[batch]
            if pattern:
                print(f"Processing Batch {batch}: {pattern}")
                processed = processor.process_batch(pattern)
            else:
                print("Processing ALL files")
                processed = processor.process_all()
            
            processor.report()
            print(f"\nFiles updated: {processed}")
        else:
            print(f"Unknown batch: {batch}")
            print("Available: 1, 2, 3, 4, 5, all")
    else:
        # Show help
        print("Qt Removal Batch Processor")
        print("\nUsage: python qt_removal_batch_processor.py [batch]")
        print("\nBatches:")
        print("  1   - qtapp/ folder (45 files)")
        print("  2   - agent/ folder (25 files)")
        print("  3   - agentic/ folder (15 files)")
        print("  4   - auth/feedback/setup/orchestration/thermal (50+ files)")
        print("  5   - training/inference/streaming (40+ files)")
        print("  all - All files in src/")
        print("\nExample: python qt_removal_batch_processor.py 1")


if __name__ == '__main__':
    main()
