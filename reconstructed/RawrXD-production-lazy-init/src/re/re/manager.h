#pragma once
#include "binary_loader.h"
#include "disassembler.h"
#include "decompiler.h"
#include "dynamic_analyzer.h"
#include "feature_flags.h"
#include "causal_graph.h"
#include <QString>
#include <QObject>

class REManager : public QObject {
    Q_OBJECT
public:
    explicit REManager(QObject *parent = nullptr);
    bool loadBinary(const QString &path);
    bool disassemble(const QString &arch);
    bool decompile();
    bool attachProcess(quint64 pid);
    bool launchProcess(const QString &path, const QStringList &args);
    QString error() const;
    // Accessors
    BinaryLoader &binaryLoader();
    Disassembler &disassembler();
    Decompiler &decompiler();
    DynamicAnalyzer &dynamicAnalyzer();
    CausalGraph &causalGraph();
private:
    BinaryLoader m_loader;
    Disassembler m_disasm;
    Decompiler m_decomp;
    DynamicAnalyzer m_dyn;
    CausalGraph m_causalGraph;
    QString m_error;
};
