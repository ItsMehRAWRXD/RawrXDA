#include "self_test.hpp"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QProcess>
#include <QStandardPaths>

SelfTest::SelfTest(QObject* parent)
    : QObject(parent) {
}

bool SelfTest::runAll() {
    m_output.clear();
    m_error.clear();

    emit log("=== Self-Test Start ===");

    if (!runUnitTests()) return false;
    if (!runIntegrationTests()) return false;
    if (!runLint()) return false;
    if (!runBenchmarkBaseline()) return false;

    emit log("=== Self-Test PASSED ===");
    return true;
}

bool SelfTest::runUnitTests() {
    emit log("Running unit tests...");

    QDir testDir(QDir::current().absoluteFilePath("build/bin"));
    if (!testDir.exists()) {
        emit log("SKIP: build/bin directory missing");
        return true;
    }

    const QFileInfoList tests = testDir.entryInfoList({"*_test.exe"}, QDir::Files);
    for (const QFileInfo& fi : tests) {
        if (!runProcess(fi.absoluteFilePath(), {}, 30000)) {
            m_error = "Unit test failed: " + fi.fileName();
            return false;
        }
    }

    emit log("Unit tests PASSED");
    return true;
}

bool SelfTest::runIntegrationTests() {
    emit log("Running integration tests...");

    struct TestCase {
        QString name;
        QString exe;
        QStringList args;
    };

    const QList<TestCase> tests = {
        {"Brutal 50 MB", "bench_deflate_50mb.exe", {}},
        {"Q8_0 end-to-end", "bench_q8_0_end2end.exe", {}},
        {"Flash-Attention", "bench_flash_attn.exe", {}},
        {"Quant ladder", "bench_quant_ladder.exe", {}}
    };

    for (const TestCase& test : tests) {
        const QString exe = QDir::current().absoluteFilePath("build/tests/" + test.exe);
        if (!QFileInfo::exists(exe)) {
            emit log("SKIP: " + test.name + " (not built)");
            continue;
        }
        if (!runProcess(exe, test.args, 60000)) {
            m_error = "Integration test failed: " + test.name;
            return false;
        }
    }

    emit log("Integration tests PASSED");
    return true;
}

bool SelfTest::runLint() {
    emit log("Running static analysis...");
    const QString cl = QStandardPaths::findExecutable("cl.exe");
    if (cl.isEmpty()) {
        emit log("SKIP: cl.exe not found in PATH - skipping static analysis");
        return true;
    }

    const QString srcDir = QDir::current().absoluteFilePath("src");
    const QStringList analyzeArgs = {"/analyze", "/W4", "/nologo"};

    for (const QString& ext : {QStringLiteral("cpp"), QStringLiteral("hpp")}) {
        QDirIterator it(srcDir, QStringList() << "*." + ext, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString file = it.next();
            if (!runProcess(cl, analyzeArgs + QStringList{QStringLiteral("/c"), file}, 30000)) {
                m_error = "Lint failed on " + file;
                return false;
            }
        }
    }

    emit log("Static analysis PASSED");
    return true;
}

bool SelfTest::runBenchmarkBaseline() {
    emit log("Running benchmark regression tests...");

    QFile db(QDir::current().absoluteFilePath("perf_db.json"));
    if (!db.open(QIODevice::ReadOnly)) {
        emit log("No baseline found - skipping regression");
        return true;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(db.readAll());
    db.close();

    if (!doc.isArray()) {
        emit log("perf_db.json format invalid - skipping regression");
        return true;
    }

    const QJsonArray arr = doc.array();
    for (const QJsonValueConstRef v : arr) {
        const QJsonObject obj = v.toObject();
        const QString name = obj.value("name").toString();
        const double baseline = obj.value("tps").toDouble();
        if (name.isEmpty() || baseline <= 0.0) {
            continue;
        }

        const QString exe = QDir::current().absoluteFilePath("build/tests/" + name + ".exe");
        if (!QFileInfo::exists(exe)) {
            emit log("SKIP: benchmark missing executable for " + name);
            continue;
        }

        const QString previousOutput = m_output;
        if (!runProcess(exe, {}, 60000)) {
            return false;
        }
        const QString newLog = m_output.mid(previousOutput.size());
        const double current = parseTPS(newLog);
        if (current < 0.0) {
            emit log("WARN: benchmark output missing TPS for " + name);
            continue;
        }
        if (!checkBenchmarkRegression(name, current, baseline)) {
            m_error = QStringLiteral("Regression in %1: %2 < %3").arg(name).arg(current).arg(baseline);
            return false;
        }
    }

    emit log("Benchmark regression PASSED");
    return true;
}

bool SelfTest::runProcess(const QString& prog, const QStringList& args, int timeoutMs) {
    QProcess proc;
    proc.setProgram(prog);
    proc.setArguments(args);
    proc.start();

    if (!proc.waitForStarted(5000)) {
        m_error = prog + " failed to start";
        return false;
    }

    if (!proc.waitForFinished(timeoutMs)) {
        proc.kill();
        m_error = prog + " timed out";
        return false;
    }

    const QString out = QString::fromUtf8(proc.readAllStandardOutput());
    const QString err = QString::fromUtf8(proc.readAllStandardError());
    m_output += out;
    m_output += err;

    if (!out.isEmpty() || !err.isEmpty()) {
        emit log(out + err);
    }

    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        m_error = prog + " failed with code " + QString::number(proc.exitCode());
        return false;
    }

    return true;
}

double SelfTest::parseTPS(const QString& log) const {
    const QStringList lines = log.split('\n', Qt::SkipEmptyParts);
    for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
        if (it->startsWith(QStringLiteral("tps:"), Qt::CaseInsensitive)) {
            return it->mid(4).trimmed().toDouble();
        }
    }
    return -1.0;
}

bool SelfTest::checkBenchmarkRegression(const QString& name, double current, double baseline) {
    Q_UNUSED(name);
    const double tolerance = 0.95; // allow 5% regression
    return current >= baseline * tolerance;
}
