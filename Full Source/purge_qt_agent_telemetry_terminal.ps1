#!/usr/bin/env pwsh
# Purge Qt from agent/, telemetry/, terminal/ directories
# Replaces all Qt types, includes, macros with C++20 STL equivalents

$ErrorActionPreference = 'Continue'
$srcRoot = "D:\rawrxd\src"
$dirs = @("$srcRoot\agent", "$srcRoot\telemetry", "$srcRoot\terminal")
$totalModified = 0

foreach ($dir in $dirs) {
    if (!(Test-Path $dir)) { Write-Host "SKIP: $dir not found"; continue }
    $files = Get-ChildItem -Path "$dir\*" -Include *.hpp,*.cpp,*.h
    foreach ($file in $files) {
        $content = [System.IO.File]::ReadAllText($file.FullName)
        $original = $content

        # ====================================================================
        # PHASE 1: Remove ALL Qt #include lines
        # ====================================================================
        $content = $content -replace '(?m)^\s*#include\s+<Q[A-Za-z/]+>\s*\r?\n', ''
        $content = $content -replace '(?m)^\s*#include\s+<Qt[A-Za-z/]+>\s*\r?\n', ''
        $content = $content -replace '(?m)^\s*#include\s+"Q[A-Za-z/]+"\s*\r?\n', ''

        # ====================================================================
        # PHASE 2: Remove Qt macros
        # ====================================================================
        $content = $content -replace '(?m)^\s*Q_OBJECT\s*\r?\n', ''
        $content = $content -replace '(?m)^\s*Q_DISABLE_COPY\([^)]*\)\s*\r?\n', ''
        $content = $content -replace '(?m)^\s*Q_DECLARE_METATYPE\([^)]*\)\s*;?\s*\r?\n', ''
        $content = $content -replace '(?m)^\s*Q_PROPERTY\([^)]*\)\s*\r?\n', ''
        $content = $content -replace '(?m)^\s*Q_INVOKABLE\s+', ''
        $content = $content -replace '(?m)^\s*Q_ENUM\([^)]*\)\s*\r?\n', ''

        # ====================================================================
        # PHASE 3: Remove QObject inheritance
        # ====================================================================
        # : public QObject { → {
        $content = $content -replace ':\s*public\s+QObject\s*\{', '{'
        # : public QObject,  → : (for multiple inheritance)
        $content = $content -replace ':\s*public\s+QObject\s*,\s*', ': '
        # : public QTcpServer { → {
        $content = $content -replace ':\s*public\s+QTcpServer\s*\{', '{'

        # ====================================================================
        # PHASE 4: Convert signals/slots sections
        # ====================================================================
        # signals: → public: // Callbacks
        $content = $content -replace '(?m)^(\s*)signals\s*:\s*$', '$1public: // Callbacks (event notifications)'
        # public slots: → public:
        $content = $content -replace '(?m)^(\s*)public\s+slots\s*:\s*$', '$1public:'
        # private slots: → private:
        $content = $content -replace '(?m)^(\s*)private\s+slots\s*:\s*$', '$1private:'
        # protected slots: → protected:
        $content = $content -replace '(?m)^(\s*)protected\s+slots\s*:\s*$', '$1protected:'

        # ====================================================================
        # PHASE 5: Remove emit keyword (just call the function directly)
        # ====================================================================
        $content = $content -replace '\bemit\s+', ''

        # ====================================================================
        # PHASE 6: Replace Qt types with STL types
        # ====================================================================

        # QString → std::string
        $content = $content -replace '\bQString\b', 'std::string'
        # QStringList → std::vector<std::string>
        $content = $content -replace '\bQStringList\b', 'std::vector<std::string>'
        # QByteArray → std::vector<uint8_t>
        $content = $content -replace '\bQByteArray\b', 'std::vector<uint8_t>'
        # QVariant → std::string (simplified)
        $content = $content -replace '\bQVariant\b', 'std::string'

        # QJsonObject → std::unordered_map<std::string, std::string>
        # Actually for JSON we need a proper type. Use a simple json_t typedef.
        $content = $content -replace '\bQJsonObject\b', 'JsonObject'
        $content = $content -replace '\bQJsonArray\b', 'JsonArray'
        $content = $content -replace '\bQJsonDocument\b', 'JsonDoc'
        $content = $content -replace '\bQJsonValue\b', 'JsonValue'

        # Container types
        $content = $content -replace '\bQVector<', 'std::vector<'
        $content = $content -replace '\bQList<', 'std::vector<'
        $content = $content -replace '\bQHash<', 'std::unordered_map<'
        $content = $content -replace '\bQMap<', 'std::map<'
        $content = $content -replace '\bQPair<', 'std::pair<'
        $content = $content -replace '\bQSet<', 'std::unordered_set<'

        # Mutex types
        $content = $content -replace '\bQMutex\b', 'std::mutex'
        $content = $content -replace '\bQMutexLocker\s+\w+\s*\(\s*&(\w+)\s*\)', 'std::lock_guard<std::mutex> lock($1)'
        $content = $content -replace '\bQRecursiveMutex\b', 'std::recursive_mutex'

        # Date/Time
        $content = $content -replace '\bQDateTime\b', 'std::chrono::system_clock::time_point'
        $content = $content -replace '\bQElapsedTimer\b', 'std::chrono::steady_clock::time_point'
        $content = $content -replace '\bQTimer\b', 'std::function<void()>/*timer*/'

        # Numeric types
        $content = $content -replace '\bqint64\b', 'int64_t'
        $content = $content -replace '\bqint32\b', 'int32_t'
        $content = $content -replace '\bquint64\b', 'uint64_t'
        $content = $content -replace '\bquint32\b', 'uint32_t'
        $content = $content -replace '\bquint8\b', 'uint8_t'
        $content = $content -replace '\bqintptr\b', 'intptr_t'

        # Network types → forward declarations / stubs
        $content = $content -replace '\bQNetworkAccessManager\b', 'void/*NetManager*/'
        $content = $content -replace '\bQNetworkRequest\b', 'void/*NetRequest*/'
        $content = $content -replace '\bQNetworkReply\b', 'void/*NetReply*/'
        $content = $content -replace '\bQUrl\b', 'std::string/*url*/'

        # Process types
        $content = $content -replace '\bQProcess\b', 'void/*Process*/'
        $content = $content -replace '\bQTcpServer\b', 'void/*TcpServer*/'
        $content = $content -replace '\bQTcpSocket\b', 'void/*TcpSocket*/'

        # File types
        $content = $content -replace '\bQFile\b', 'std::fstream'
        $content = $content -replace '\bQDir\b', 'std::filesystem::path'
        $content = $content -replace '\bQFileInfo\b', 'std::filesystem::path'
        $content = $content -replace '\bQFileInfoList\b', 'std::vector<std::filesystem::path>'
        $content = $content -replace '\bQDirIterator\b', 'std::filesystem::recursive_directory_iterator'
        $content = $content -replace '\bQFileSystemWatcher\b', 'void/*FSWatcher*/'
        $content = $content -replace '\bQTextStream\b', 'std::ostringstream'
        $content = $content -replace '\bQIODevice\b', 'std::ios'

        # Other Qt types
        $content = $content -replace '\bQRegularExpression\b', 'std::regex'
        $content = $content -replace '\bQRegularExpressionMatch\b', 'std::smatch'
        $content = $content -replace '\bQUuid\b', 'std::string/*uuid*/'
        $content = $content -replace '\bQCryptographicHash\b', 'void/*CryptoHash*/'
        $content = $content -replace '\bQMessageAuthenticationCode\b', 'void/*HMAC*/'
        $content = $content -replace '\bQSettings\b', 'void/*Settings*/'
        $content = $content -replace '\bQStandardPaths\b', 'std::filesystem::path'
        $content = $content -replace '\bQSysInfo\b', 'void/*SysInfo*/'
        $content = $content -replace '\bQCoreApplication\b', 'void/*App*/'
        $content = $content -replace '\bQApplication\b', 'void/*App*/'
        $content = $content -replace '\bQThread\b', 'std::thread'
        $content = $content -replace '\bQFuture\b', 'std::future'
        $content = $content -replace '\bQEventLoop\b', 'void/*EventLoop*/'
        $content = $content -replace '\bQProcessEnvironment\b', 'void/*ProcEnv*/'
        $content = $content -replace '\bQClipboard\b', 'void/*Clipboard*/'
        $content = $content -replace '\bQInputDialog\b', 'void/*InputDialog*/'
        $content = $content -replace '\bQMessageBox\b', 'void/*MsgBox*/'
        $content = $content -replace '\bQCommandLineParser\b', 'void/*CmdParser*/'
        $content = $content -replace '\bQSharedPointer\b', 'std::shared_ptr'
        $content = $content -replace '\bQScopedPointer\b', 'std::unique_ptr'
        $content = $content -replace '\bQMetaType\b', 'void/*MetaType*/'
        $content = $content -replace '\bQSqlDatabase\b', 'void/*SqlDb*/'
        $content = $content -replace '\bQSqlQuery\b', 'void/*SqlQuery*/'
        $content = $content -replace '\bQSqlError\b', 'void/*SqlError*/'

        # ====================================================================
        # PHASE 7: Replace Qt function calls with STL equivalents
        # ====================================================================

        # qDebug() << ... → fprintf(stderr, ...)  — replace with a simple log macro placeholder
        # We'll replace qDebug/qWarning/qCritical with fprintf-based logging
        $content = $content -replace '\bqDebug\(\)\s*<<\s*', 'fprintf(stderr, "%s\\n", std::string('
        $content = $content -replace '\bqWarning\(\)\s*<<\s*', 'fprintf(stderr, "[WARN] %s\\n", std::string('
        $content = $content -replace '\bqCritical\(\)\s*<<\s*', 'fprintf(stderr, "[CRIT] %s\\n", std::string('
        # Actually these chained << operators are hard to replace mechanically.
        # Let's just convert qDebug() to a comment/noop for now
        $content = $content -replace '\bqDebug\(\)', '/* qDebug removed */'
        $content = $content -replace '\bqWarning\(\)', '/* qWarning removed */'
        $content = $content -replace '\bqCritical\(\)', '/* qCritical removed */'
        $content = $content -replace '\bqDebug\b(?!\()', '/* qDebug removed */'
        $content = $content -replace '\bqDeleteAll\b', '/* qDeleteAll */'

        # QObject constructor patterns
        # : QObject(parent)  → remove from initializer list
        $content = $content -replace ',\s*QObject\(parent\)', ''
        $content = $content -replace ':\s*QObject\(parent\)\s*,', ':'
        $content = $content -replace ':\s*QObject\(parent\)\s*\{', '{'
        $content = $content -replace ':\s*QObject\(parent\)\s*$', ''

        # QObject* parent = nullptr → remove from parameters
        $content = $content -replace ',\s*QObject\s*\*\s*parent\s*=\s*nullptr', ''
        $content = $content -replace 'QObject\s*\*\s*parent\s*=\s*nullptr\s*,\s*', ''
        $content = $content -replace 'QObject\s*\*\s*parent\s*=\s*nullptr', ''
        $content = $content -replace 'QObject\s*\*\s*parent', ''

        # Remove override for QObject virtual methods
        # ~ClassName() override → ~ClassName()  (keep override only if there's still a base class)
        # Actually let's keep override, it doesn't hurt

        # connect() calls → comment out (need manual conversion to callbacks)
        $content = $content -replace '\bconnect\(\s*m_', '/* connect( m_'
        $content = $content -replace '&QTimer::timeout', '&timeout */ /* FIXME: wire callback'
        # Generic connect pattern
        $content = $content -replace '(?m)(\s*)connect\(([^;]*);', '$1/* FIXME: convert to callback: connect($2; */'

        # QtConcurrent::run → std::async
        $content = $content -replace '\bQtConcurrent::run\b', 'std::async(std::launch::async, '

        # QJsonArray::fromStringList → direct vector construction
        $content = $content -replace '\bJsonArray::fromStringList\b', '/* fromStringList */'

        # String method conversions from Qt to STL
        $content = $content -replace '\.isEmpty\(\)', '.empty()'
        $content = $content -replace '\.toUtf8\(\)', '/* .c_str() */'
        $content = $content -replace '\.toStdString\(\)', ''
        $content = $content -replace '\.toLatin1\(\)', '/* .c_str() */'
        $content = $content -replace '\.arg\(', ' /* .arg( */'
        $content = $content -replace '\bstd::string::number\(', 'std::to_string('
        $content = $content -replace '\.noquote\(\)', ''

        # QProcess state checks
        $content = $content -replace 'void/\*Process\*/::Running', '/* ProcessRunning */'
        $content = $content -replace 'void/\*Process\*/::NotRunning', '/* ProcessNotRunning */'
        $content = $content -replace 'void/\*Process\*/::ExitStatus', 'int'
        $content = $content -replace '::Compact\b', '/* Compact */'

        # QDir static methods
        $content = $content -replace '\bstd::filesystem::path::currentPath\(\)', 'std::filesystem::current_path().string()'
        $content = $content -replace '\bstd::filesystem::path::separator\(\)', 'std::filesystem::path::preferred_separator'

        # QFileInfo static methods
        $content = $content -replace '\bstd::filesystem::path::exists\b', 'std::filesystem::exists'

        # QFile static methods
        $content = $content -replace '\bstd::fstream::copy\b', 'std::filesystem::copy_file'
        $content = $content -replace '\bstd::fstream::remove\b', 'std::filesystem::remove'
        $content = $content -replace '\bstd::fstream::rename\b', 'std::filesystem::rename'

        # QDateTime static methods
        $content = $content -replace '\bstd::chrono::system_clock::time_point::currentDateTime\(\)', 'std::chrono::system_clock::now()'
        $content = $content -replace '\bstd::chrono::system_clock::time_point::currentDateTimeUtc\(\)', 'std::chrono::system_clock::now()'

        # Qt::ISODateWithMs → remove
        $content = $content -replace '\bQt::ISODateWithMs\b', '/* ISO8601 */'
        $content = $content -replace '\bQt::ISODate\b', '/* ISO8601 */'

        # Remove remaining Qt:: namespace references
        $content = $content -replace '\bQt::\w+', '0 /* Qt flag removed */'

        # QIODevice flags
        $content = $content -replace '\bstd::ios::WriteOnly\b', 'std::ios::out'
        $content = $content -replace '\bstd::ios::ReadOnly\b', 'std::ios::in'
        $content = $content -replace '\bstd::ios::Append\b', 'std::ios::app'
        $content = $content -replace '\bstd::ios::Text\b', '0'

        # Remove QDir::Files, QDir::Name etc.
        $content = $content -replace '\bstd::filesystem::path::Files\b', '/* Files */'
        $content = $content -replace '\bstd::filesystem::path::Name\b', '/* Name */'

        # ====================================================================
        # PHASE 8: Add required STL includes after #pragma once or first include
        # ====================================================================
        # Only add if they're not already present
        $needsIncludes = @()
        if ($content -match '\bstd::string\b' -and $content -notmatch '#include\s+<string>') { $needsIncludes += '#include <string>' }
        if ($content -match '\bstd::vector\b' -and $content -notmatch '#include\s+<vector>') { $needsIncludes += '#include <vector>' }
        if ($content -match '\bstd::unordered_map\b' -and $content -notmatch '#include\s+<unordered_map>') { $needsIncludes += '#include <unordered_map>' }
        if ($content -match '\bstd::map\b' -and $content -notmatch '#include\s+<map>') { $needsIncludes += '#include <map>' }
        if ($content -match '\bstd::unordered_set\b' -and $content -notmatch '#include\s+<unordered_set>') { $needsIncludes += '#include <unordered_set>' }
        if ($content -match '\bstd::mutex\b|std::lock_guard' -and $content -notmatch '#include\s+<mutex>') { $needsIncludes += '#include <mutex>' }
        if ($content -match '\bstd::regex\b|std::smatch' -and $content -notmatch '#include\s+<regex>') { $needsIncludes += '#include <regex>' }
        if ($content -match '\bstd::chrono\b' -and $content -notmatch '#include\s+<chrono>') { $needsIncludes += '#include <chrono>' }
        if ($content -match '\bstd::filesystem\b' -and $content -notmatch '#include\s+<filesystem>') { $needsIncludes += '#include <filesystem>' }
        if ($content -match '\bstd::fstream\b' -and $content -notmatch '#include\s+<fstream>') { $needsIncludes += '#include <fstream>' }
        if ($content -match '\bstd::ostringstream\b' -and $content -notmatch '#include\s+<sstream>') { $needsIncludes += '#include <sstream>' }
        if ($content -match '\bstd::pair\b' -and $content -notmatch '#include\s+<utility>') { $needsIncludes += '#include <utility>' }
        if ($content -match '\bstd::thread\b' -and $content -notmatch '#include\s+<thread>') { $needsIncludes += '#include <thread>' }
        if ($content -match '\bstd::future\b|std::async\b' -and $content -notmatch '#include\s+<future>') { $needsIncludes += '#include <future>' }
        if ($content -match '\bstd::function\b' -and $content -notmatch '#include\s+<functional>') { $needsIncludes += '#include <functional>' }
        if ($content -match '\bstd::shared_ptr\b|std::unique_ptr\b|std::make_unique\b|std::make_shared\b' -and $content -notmatch '#include\s+<memory>') { $needsIncludes += '#include <memory>' }
        if ($content -match '\bint64_t\b|uint64_t|int32_t|uint32_t|uint8_t|intptr_t' -and $content -notmatch '#include\s+<cstdint>') { $needsIncludes += '#include <cstdint>' }
        if ($content -match '\bfprintf\b|stderr' -and $content -notmatch '#include\s+<cstdio>') { $needsIncludes += '#include <cstdio>' }
        if ($content -match '\bstd::to_string\b' -and $content -notmatch '#include\s+<string>') { $needsIncludes += '#include <string>' }
        if ($content -match '\bJsonObject\b|JsonArray\b|JsonDoc\b|JsonValue\b') {
            if ($content -notmatch '#include\s+"json_types\.hpp"' -and $content -notmatch '#include\s+<json_types\.hpp>') {
                $needsIncludes += '#include "json_types.hpp"'
            }
        }

        if ($needsIncludes.Count -gt 0) {
            $includeBlock = ($needsIncludes | Sort-Object -Unique) -join "`n"
            # Insert after #pragma once
            if ($content -match '(?m)^#pragma once\s*$') {
                $content = $content -replace '(?m)^(#pragma once)\s*$', "`$1`n$includeBlock"
            }
            # Or after first #include
            elseif ($content -match '(?m)^(#include\s+[^\r\n]+)') {
                $firstInclude = $Matches[0]
                $idx = $content.IndexOf($firstInclude) + $firstInclude.Length
                $lineEnd = $content.IndexOf("`n", $idx)
                if ($lineEnd -lt 0) { $lineEnd = $content.Length }
                $content = $content.Insert($lineEnd + 1, "$includeBlock`n")
            }
        }

        # ====================================================================
        # PHASE 9: Clean up artifacts
        # ====================================================================
        # Remove double blank lines
        $content = $content -replace '(\r?\n){3,}', "`n`n"
        # Remove dangling override on destructors if no base class
        # (handled case by case if needed)

        # Write if changed
        if ($content -ne $original) {
            [System.IO.File]::WriteAllText($file.FullName, $content)
            $totalModified++
            Write-Host "MODIFIED: $($file.FullName)"
        } else {
            Write-Host "unchanged: $($file.FullName)"
        }
    }
}

Write-Host "`n=== Qt Purge Complete: $totalModified files modified ==="
