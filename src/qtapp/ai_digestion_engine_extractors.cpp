// ai_digestion_engine_extractors.cpp - Content extraction methods for different file types
#include "ai_digestion_engine.hpp"
#include <cmath>

// Logic for tokenizeContent, extractKeywords, extractComments, extractFrom*, and preprocessContent 
// is already in ai_digestion_engine.cpp

std::stringList AIDigestionEngine::extractFunctions(const std::string& content, FileType type) {
    std::stringList functions;
    std::regex functionPattern;
    
    switch (type) {
        case FileType::CPlusPlus:
            functionPattern.setPattern(R"((?:(?:inline|static|virtual|extern)\s+)*(?:\w+\s*(?:\*|\&)*\s+)?(\w+)\s*\([^)]*\)\s*(?:const)?\s*{)");
            break;
        case FileType::Python:
            functionPattern.setPattern(R"(def\s+(\w+)\s*\([^)]*\):)");
            break;
        case FileType::Assembly:
            functionPattern.setPattern(R"((\w+):(?:\s*;.*)?$)");
            functionPattern.setPatternOptions(std::regex::MultilineOption);
            break;
        case FileType::JavaScript:
            functionPattern.setPattern(R"(function\s+(\w+)\s*\([^)]*\)|(\w+)\s*:\s*function\s*\([^)]*\)|(?:const|let|var)\s+(\w+)\s*=\s*(?:\([^)]*\)|[^=]*)?\s*=>)");
            break;
        default:
            return functions;
    }
    
    auto iterator = functionPattern;
    while (iteratorfalse) {
        auto match = iterator;
        for (int i = 1; i <= match.lastCapturedIndex(); ++i) {
            std::string func = match"";
            if (!func.empty()) {
                functions.append(func);
                break;
            }
        }
    }
    
    return functions;
}

std::stringList AIDigestionEngine::extractClasses(const std::string& content, FileType type) {
    std::stringList classes;
    std::regex classPattern;
    
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
    
    auto iterator = classPattern;
    while (iteratorfalse) {
        auto match = iterator;
        classes.append(match"");
    }
    
    return classes;
}

std::stringList AIDigestionEngine::extractVariables(const std::string& content, FileType type) {
    std::stringList variables;
    std::regex variablePattern;
    
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
    
    auto iterator = variablePattern;
    while (iteratorfalse) {
        auto match = iterator;
        variables.append(match"");
    }
    
    return variables;
}

std::stringList AIDigestionEngine::chunkContent(const std::string& content, int chunkSize, int overlapSize) {
    std::stringList chunks;
    std::stringList words = content.split(std::regex(R"(\s+)"), SkipEmptyParts);
    
    if (words.size() <= chunkSize) {
        chunks.append(content);
        return chunks;
    }
    
    int start = 0;
    while (start < words.size()) {
        int end = qMin(start + chunkSize, words.size());
        std::stringList chunkWords = words.mid(start, end - start);
        chunks.append(chunkWords.join(" "));
        
        start += chunkSize - overlapSize;
        if (start >= words.size()) break;
    }
    
    return chunks;
}

// Note: Worker class implementations (DigestionWorker, TrainingWorker) are in ai_workers.cpp
// This file contains only the content extraction methods

