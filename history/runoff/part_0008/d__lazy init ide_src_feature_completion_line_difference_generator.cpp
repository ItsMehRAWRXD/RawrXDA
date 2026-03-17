// ═══════════════════════════════════════════════════════════════════════════════
// LINE DIFFERENCE GENERATOR - IMPLEMENTATION
// Counts how far across on a single line and generates differences for every line
// ═══════════════════════════════════════════════════════════════════════════════

#include "line_difference_generator.h"
#include <QTextStream>
#include <QFile>
#include <QRegularExpression>
#include <math>

LineDifferenceGenerator::LineDifferenceGenerator(QObject* parent)
    : QObject(parent)
    , m_contextRadius(3)
    , m_includeWhitespace(false)
    , m_minChangeThreshold(0.01)
{
}

FileDifference LineDifferenceGenerator::generateDifferences(const QString& originalFile, const QString& modifiedFile)
{
    QFile origFile(originalFile);
    QFile modFile(modifiedFile);
    
    if (!origFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[LineDifferenceGenerator] Failed to open original file:" << originalFile;
        return FileDifference();
    }
    
    if (!modFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[LineDifferenceGenerator] Failed to open modified file:" << modifiedFile;
        origFile.close();
        return FileDifference();
    }
    
    QTextStream origStream(&origFile);
    QTextStream modStream(&modFile);
    
    QString origContent = origStream.readAll();
    QString modContent = modStream.readAll();
    
    origFile.close();
    modFile.close();
    
    return generateDifferencesFromStrings(origContent, modContent);
}

FileDifference LineDifferenceGenerator::generateDifferencesFromStrings(
    const QString& originalContent, const QString& modifiedContent)
{
    FileDifference result;
    result.totalLines = 0;
    result.changedLines = 0;
    result.totalCharacters = 0;
    result.changedCharacters = 0;
    
    QStringList origLines = originalContent.split('\n');
    QStringList modLines = modifiedContent.split('\n');
    
    int maxLines = qMax(origLines.size(), modLines.size());
    result.totalLines = maxLines;
    
    for (int i = 0; i < maxLines; ++i) {
        QString origLine = i < origLines.size() ? origLines[i] : "";
        QString modLine = i < modLines.size() ? modLines[i] : "";
        
        LineDifference lineDiff = analyzeLineDifference(i + 1, origLine, modLine);
        
        if (lineDiff.changedCharacters > 0) {
            result.changedLines++;
            result.changedCharacters += lineDiff.changedCharacters;
        }
        
        result.totalCharacters += qMax(origLine.length(), modLine.length());
        result.lineDifferences.append(lineDiff);
        
        emit lineAnalyzed(i + 1, (i * 100) / maxLines);
    }
    
    result.overallChangePercentage = result.totalCharacters > 0 ? 
        (static_cast<double>(result.changedCharacters) / result.totalCharacters) * 100.0 : 0.0;
    
    result.unifiedDiff = generateUnifiedDiff(result);
    
    emit fileAnalyzed(result.filePath, result.changedLines);
    return result;
}

LineDifference LineDifferenceGenerator::analyzeLineDifference(
    int lineNumber, const QString& originalLine, const QString& modifiedLine)
{
    LineDifference result;
    result.lineNumber = lineNumber;
    result.originalLine = originalLine;
    result.modifiedLine = modifiedLine;
    
    int maxLength = qMax(originalLine.length(), modifiedLine.length());
    result.totalCharacters = maxLength;
    result.changedCharacters = 0;
    
    // Analyze character positions
    result.characterPositions = analyzeCharacterPositions(originalLine, lineNumber);
    
    // Compare character by character
    for (int i = 0; i < maxLength; ++i) {
        QChar origChar = i < originalLine.length() ? originalLine[i] : QChar();
        QChar modChar = i < modifiedLine.length() ? modifiedLine[i] : QChar();
        
        if (origChar != modChar && isSignificantChange(QString(origChar), QString(modChar))) {
            result.changedCharacters++;
            result.changedPositions.append(i);
        }
    }
    
    result.changePercentage = maxLength > 0 ? 
        (static_cast<double>(result.changedCharacters) / maxLength) * 100.0 : 0.0;
    
    result.diffString = generateDiffString(originalLine, modifiedLine);
    result.characterMap = generateCharacterMap(result);
    
    return result;
}

QVector<CharacterPosition> LineDifferenceGenerator::analyzeCharacterPositions(
    const QString& line, int lineNumber)
{
    QVector<CharacterPosition> positions;
    
    for (int i = 0; i < line.length(); ++i) {
        CharacterPosition pos;
        pos.lineNumber = lineNumber;
        pos.characterIndex = i;
        pos.character = line[i];
        pos.context = getContext(line, i, m_contextRadius);
        pos.entropy = calculatePositionEntropy(line, i);
        pos.distanceFromStart = i;
        pos.distanceFromEnd = line.length() - i - 1;
        pos.normalizedPosition = line.length() > 0 ? static_cast<double>(i) / line.length() : 0.0;
        
        positions.append(pos);
    }
    
    return positions;
}

QString LineDifferenceGenerator::generateUnifiedDiff(const FileDifference& diff)
{
    QString result;
    QTextStream stream(&result);
    
    stream << "--- a/" << diff.filePath << "\n";
    stream << "+++ b/" << diff.filePath << "\n";
    
    for (const auto& lineDiff : diff.lineDifferences) {
        if (lineDiff.changedCharacters > 0) {
            stream << "@@ -" << lineDiff.lineNumber << " +" << lineDiff.lineNumber << " @@\n";
            stream << "-" << escapeForDiff(lineDiff.originalLine) << "\n";
            stream << "+" << escapeForDiff(lineDiff.modifiedLine) << "\n";
        }
    }
    
    return result;
}

QString LineDifferenceGenerator::generateCharacterMap(const LineDifference& lineDiff)
{
    QString result;
    int maxLength = qMax(lineDiff.originalLine.length(), lineDiff.modifiedLine.length());
    
    for (int i = 0; i < maxLength; ++i) {
        QChar origChar = i < lineDiff.originalLine.length() ? lineDiff.originalLine[i] : QChar(' ');
        QChar modChar = i < lineDiff.modifiedLine.length() ? lineDiff.modifiedLine[i] : QChar(' ');
        
        if (origChar == modChar) {
            result += "·";  // Unchanged
        } else {
            result += "█";  // Changed
        }
    }
    
    return result;
}

double LineDifferenceGenerator::calculatePositionEntropy(const QString& line, int position)
{
    if (line.isEmpty() || position < 0 || position >= line.length()) {
        return 0.0;
    }
    
    // Calculate character frequency in context window
    int start = qMax(0, position - m_contextRadius);
    int end = qMin(line.length() - 1, position + m_contextRadius);
    
    QHash<QChar, int> frequency;
    int total = 0;
    
    for (int i = start; i <= end; ++i) {
        if (!m_includeWhitespace && line[i].isSpace()) {
            continue;
        }
        frequency[line[i]]++;
        total++;
    }
    
    if (total == 0) return 0.0;
    
    // Calculate entropy
    double entropy = 0.0;
    for (auto it = frequency.begin(); it != frequency.end(); ++it) {
        double probability = static_cast<double>(it.value()) / total;
        entropy -= probability * std::log2(probability);
    }
    
    return entropy;
}

QString LineDifferenceGenerator::getContext(const QString& line, int position, int radius)
{
    int start = qMax(0, position - radius);
    int end = qMin(line.length() - 1, position + radius);
    
    QString context;
    for (int i = start; i <= end; ++i) {
        context += line[i];
    }
    
    return context;
}

bool LineDifferenceGenerator::isSignificantChange(const QString& origChar, const QString& modChar)
{
    if (origChar == modChar) return false;
    
    // Ignore whitespace changes if configured
    if (!m_includeWhitespace && 
        (origChar.trimmed().isEmpty() || modChar.trimmed().isEmpty())) {
        return false;
    }
    
    return true;
}

QString LineDifferenceGenerator::escapeForDiff(const QString& text)
{
    QString result = text;
    result.replace("\\", "\\\\");
    result.replace("\n", "\\n");
    result.replace("\t", "\\t");
    return result;
}

QString LineDifferenceGenerator::generateDiffString(const QString& original, const QString& modified)
{
    QString result;
    int maxLength = qMax(original.length(), modified.length());
    
    for (int i = 0; i < maxLength; ++i) {
        QChar origChar = i < original.length() ? original[i] : QChar();
        QChar modChar = i < modified.length() ? modified[i] : QChar();
        
        if (origChar == modChar) {
            result += origChar;
        } else {
            result += QString("[") + origChar + "->" + modChar + "]";
        }
    }
    
    return result;
}
