#pragma once
#include <QString>
#include <QVector>
#include "disassembler.h"

struct DecompiledFunction {
    QString name;
    QString pseudoC;
    quint64 address;
    QVector<QString> calledFunctions;
};

class Decompiler {
public:
    bool decompile(const QVector<DisasmInstruction> &instructions);
    QVector<DecompiledFunction> functions() const;
    QString error() const;
private:
    QVector<DecompiledFunction> m_functions;
    QString m_error;
};
