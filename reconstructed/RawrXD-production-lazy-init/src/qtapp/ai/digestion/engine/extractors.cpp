// ai_digestion_engine_extractors.cpp - Content extraction methods for different file types
#include "ai_digestion_engine.hpp"
#include <QtCore/QRegularExpression>
#include <QtCore/QTextStream>
#include <QtCore/QUuid>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QDateTime>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QThread>
#include <cmath>

// Logic for tokenizeContent, extractKeywords, extractComments, extractFrom*, and preprocessContent 
// is already in ai_digestion_engine.cpp

QStringList AIDigestionEngine::extractFunctions(const QString& content, FileType type) {
    QStringList functions;
    QRegularExpression functionPattern;
    
    switch (type) {
        case FileType::CPlusPlus:
            functionPattern.setPattern(R"((?:(?:inline|static|virtual|extern)\s+)*(?:\w+\s*(?:\*|\&)*\s+)?(\w+)\s*\([^)]*\)\s*(?:const)?\s*{)");
            break;
        case FileType::Python:
            functionPattern.setPattern(R"(def\s+(\w+)\s*\([^)]*\):)");
            break;
        case FileType::Assembly:
            functionPattern.setPattern(R"((\w+):(?:\s*;.*)?$)");
            functionPattern.setPatternOptions(QRegularExpression::MultilineOption);
            break;
        case FileType::JavaScript:
            functionPattern.setPattern(R"(function\s+(\w+)\s*\([^)]*\)|(\w+)\s*:\s*function\s*\([^)]*\)|(?:const|let|var)\s+(\w+)\s*=\s*(?:\([^)]*\)|[^=]*)?\s*=>)");
            break;
        default:
            return functions;
    }
    
    auto iterator = functionPattern.globalMatch(content);
    while (iterator.hasNext()) {
        auto match = iterator.next();
        for (int i = 1; i <= match.lastCapturedIndex(); ++i) {
            QString func = match.captured(i);
            if (!func.isEmpty()) {
                functions.append(func);
                break;
            }
        }
    }
    
    return functions;
}

QStringList AIDigestionEngine::extractClasses(const QString& content, FileType type) {
    QStringList classes;
    QRegularExpression classPattern;
    
    switch (type) {
        case FileType::CPlusPlus:
            classPattern.setPattern(R"((?:class|struct)\s+(\w+)(?:\s*:\s*(?:public|private|protected)\s+\w+)?(?:\s*{|;))");
            break;
        case FileType::Python:
            classPattern.setPattern(R"(class\s+(\w+)(?:\([^)]*\))?:)");
            break;
        default:
            return classes;
    }
    
    auto iterator = classPattern.globalMatch(content);
    while (iterator.hasNext()) {
        auto match = iterator.next();
        classes.append(match.captured(1));
    }
    
    return classes;
}

QStringList AIDigestionEngine::extractVariables(const QString& content, FileType type) {
    QStringList variables;
    QRegularExpression variablePattern;
    
    switch (type) {
        case FileType::CPlusPlus:
            variablePattern.setPattern(R"((?:(?:static|const|extern|mutable)\s+)*(?:\w+\s*(?:\*|\&)*\s+)+(\w+)(?:\s*=|\s*;|\s*\[))");
            break;
        case FileType::Python:
            variablePattern.setPattern(R"((?:^|\s)(\w+)\s*=(?!=))");
            break;
        case FileType::Assembly:
            variablePattern.setPattern(R"((\w+)\s+(?:db|dw|dd|dq|equ))");
            break;
        default:
            return variables;
    }
    
    auto iterator = variablePattern.globalMatch(content);
    while (iterator.hasNext()) {
        auto match = iterator.next();
        variables.append(match.captured(1));
    }
    
    return variables;
}

QStringList AIDigestionEngine::chunkContent(const QString& content, int chunkSize, int overlapSize) {
    QStringList chunks;
    QStringList words = content.split(QRegularExpression(R"(\s+)"), Qt::SkipEmptyParts);
    
    if (words.size() <= chunkSize) {
        chunks.append(content);
        return chunks;
    }
    
    int start = 0;
    while (start < words.size()) {
        int end = qMin(start + chunkSize, words.size());
        QStringList chunkWords = words.mid(start, end - start);
        chunks.append(chunkWords.join(" "));
        
        start += chunkSize - overlapSize;
        if (start >= words.size()) break;
    }
    
    return chunks;
}

// Note: Worker class implementations (DigestionWorker, TrainingWorker) are in ai_workers.cpp
// This file contains only the content extraction methods
