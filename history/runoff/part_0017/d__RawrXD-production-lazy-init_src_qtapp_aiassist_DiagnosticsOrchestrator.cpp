#include "DiagnosticsOrchestrator.h"

#include <QRegularExpression>

DiagnosticsOrchestrator::DiagnosticsOrchestrator(QObject* parent) : QObject(parent) {}
DiagnosticsOrchestrator::~DiagnosticsOrchestrator() {}

DiagnosticsOrchestrator::Diagnostic DiagnosticsOrchestrator::makeDiag(const QString& filePath, int line, Diagnostic::Severity sev, const QString& msg, const QString& rule) const {
    Diagnostic d;
    d.filePath = filePath;
    d.line = line;
    d.severity = sev;
    d.message = msg;
    d.ruleId = rule;
    return d;
}

QList<DiagnosticsOrchestrator::Diagnostic> DiagnosticsOrchestrator::runRules(const QString& filePath, const QStringList& lines) const {
    QList<Diagnostic> out;
    QRegularExpression trailingWhitespace("\\s+$");
    QRegularExpression todo("TODO|FIXME");
    QRegularExpression longLine("^.{121,}$");

    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];
        int lineNo = i + 1;
        if (trailingWhitespace.match(line).hasMatch()) {
            out.append(makeDiag(filePath, lineNo, Diagnostic::Warning, "Trailing whitespace", "style.trailing_whitespace"));
        }
        if (todo.match(line).hasMatch()) {
            out.append(makeDiag(filePath, lineNo, Diagnostic::Info, "TODO/FIXME present", "style.todo_comment"));
        }
        if (longLine.match(line).hasMatch()) {
            out.append(makeDiag(filePath, lineNo, Diagnostic::Warning, "Line exceeds 120 chars", "style.line_length"));
        }
        if (line.contains("using namespace std")) {
            out.append(makeDiag(filePath, lineNo, Diagnostic::Warning, "Avoid 'using namespace std'", "modern.cpp.using_namespace_std"));
        }
        if (line.contains("strcpy(")) {
            out.append(makeDiag(filePath, lineNo, Diagnostic::Error, "Potential unsafe strcpy", "security.cstr.strcpy"));
        }
    }
    return out;
}

QList<DiagnosticsOrchestrator::Diagnostic> DiagnosticsOrchestrator::analyze(const QString& filePath, const QString& content) {
    QStringList lines = content.split('\n');
    QList<Diagnostic> diags = runRules(filePath, lines);
    emit diagnosticsReady(filePath, diags);
    return diags;
}

QMap<QString, QList<DiagnosticsOrchestrator::Diagnostic>> DiagnosticsOrchestrator::analyzeProject(const QMap<QString, QString>& files) {
    QMap<QString, QList<Diagnostic>> results;
    for (auto it = files.begin(); it != files.end(); ++it) {
        results[it.key()] = analyze(it.key(), it.value());
    }
    return results;
}
