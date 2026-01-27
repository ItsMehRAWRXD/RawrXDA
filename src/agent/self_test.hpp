#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class SelfTest : public QObject {
    Q_OBJECT
public:
    explicit SelfTest(QObject* parent = nullptr);

    bool runAll();               // unit + integration + perf
    bool runUnitTests();         // build/bin/*_test.exe
    bool runIntegrationTests();  // deflate_50mb, flash_attn, etc.
    bool runLint();              // cl.exe /analyze
    bool runBenchmarkBaseline(); // tokens/sec vs. stored baseline

    QString lastOutput() const { return m_output; }
    QString lastError() const { return m_error; }

signals:
    void log(const QString& line);

private:
    bool runProcess(const QString& prog, const QStringList& args, int timeoutMs = 60000);
    double parseTPS(const QString& log) const;
    bool checkBenchmarkRegression(const QString& name, double current, double baseline);

    QString m_output;
    QString m_error;
};
