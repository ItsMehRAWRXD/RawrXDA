#pragma once
#include <QString>
#include <QByteArray>
#include <QVector>

struct DisasmInstruction {
    quint64 address;
    QString mnemonic;
    QString operands;
    QString comment;
};

class Disassembler {
public:
    bool disassemble(const QByteArray &code, quint64 baseAddress, const QString &arch);
    QVector<DisasmInstruction> instructions() const;
    QString error() const;
private:
    QVector<DisasmInstruction> m_instructions;
    QString m_error;
};
