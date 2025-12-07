#include "auto_bootstrap.hpp"
#include "planner.hpp"
#include "self_patch.hpp"
#include "release_agent.hpp"
#include "meta_learn.hpp"
#include "zero_touch.hpp"
#include <QCoreApplication>
#include <QApplication>
#include <QTimer>
#include <QProcessEnvironment>
#include <QInputDialog>
#include <QMessageBox>
#include <QClipboard>
#include <QJsonArray>
#include <QJsonObject>
#include <QtConcurrent>
#include <QProcess>
#include <QDebug>

AutoBootstrap* AutoBootstrap::s_instance = nullptr;

AutoBootstrap* AutoBootstrap::instance() {
    if (!s_instance) {
        s_instance = new AutoBootstrap(qApp);
    }
    return s_instance;
}

AutoBootstrap::AutoBootstrap(QObject* parent) : QObject(parent) {}

void AutoBootstrap::installZeroTouch() {
    static ZeroTouch* zero = nullptr;
    if (zero) {
        return;
    }
    zero = new ZeroTouch(instance());
    zero->installAll();
}

void AutoBootstrap::startWithWish(const QString& wish) {
    instance()->startWithWishInternal(wish);
}

QString AutoBootstrap::grabWish() {
    // 1. Environment variable (CI / voice assistant / automation)
    QString env = QProcessEnvironment::systemEnvironment().value("RAWRXD_WISH");
    if (!env.isEmpty()) {
        qDebug() << "Wish from env-var:" << env;
        return env;
    }
    
    // 2. Clipboard (Windows speech recognition leaves it here)
    QClipboard* clip = QApplication::clipboard();
    QString spoken = clip->text(QClipboard::Selection);
    if (spoken.isEmpty()) {
        spoken = clip->text(QClipboard::Clipboard);
    }
    
    if (!spoken.isEmpty() && spoken.length() < 200 && !spoken.contains('\n')) {
        qDebug() << "Wish from clipboard:" << spoken;
        return spoken;
    }
    
    // 3. Dialog (fallback for desktop)
    bool ok;
    QString typed = QInputDialog::getText(
        nullptr, 
        "RawrXD Agent", 
        "What should I build / fix / ship?",
        QLineEdit::Normal,
        "",
        &ok
    );
    
    if (ok && !typed.isEmpty()) {
        qDebug() << "Wish from dialog:" << typed;
        return typed;
    }
    
    return QString();
}

void AutoBootstrap::startWithWishInternal(const QString& wish) {
    if (wish.isEmpty()) {
        qDebug() << "No wish received, aborting";
        return;
    }

    emit wishReceived(wish);

    if (!safetyGate(wish)) {
        qDebug() << "Safety gate rejected wish";
        return;
    }

    Planner planner;
    QJsonArray plan = planner.plan(wish);

    if (plan.isEmpty()) {
        QMessageBox::warning(nullptr, "Agent", "I don't know how to do that yet.");
        emit executionCompleted(false);
        return;
    }

    executePlan(wish, plan);
}

bool AutoBootstrap::safetyGate(const QString& wish) {
    // Blacklist dangerous operations
    QStringList blacklist = {
        "rm -rf", "format", "del /", "shutdown", 
        "powershell -c \"rm", "remove-item -recurse",
        "dd if=/dev/zero", "mkfs"
    };
    
    QString lowerWish = wish.toLower();
    for (const QString& word : blacklist) {
        if (lowerWish.contains(word.toLower())) {
            QMessageBox::critical(
                nullptr, 
                "Agent Safety", 
                QString("Blocked dangerous operation: %1").arg(word)
            );
            return false;
        }
    }
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString autoApprove = env.value("RAWRXD_AUTO_APPROVE");
    bool ciContext = env.value("CI").compare("true", Qt::CaseInsensitive) == 0 || env.contains("GITHUB_ACTIONS");
    if (autoApprove.compare("1", Qt::CaseInsensitive) == 0 ||
        autoApprove.compare("true", Qt::CaseInsensitive) == 0 ||
        ciContext) {
        qDebug() << "Safety gate auto-approved";
        return true;
    }
    
    // Ask user confirmation
    QString message = QString("Autonomously execute:\n\n%1\n\nProceed?").arg(wish);
    int result = QMessageBox::question(
        nullptr, 
        "Agent Launch", 
        message,
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );
    
    return (result == QMessageBox::Yes);
}

void AutoBootstrap::executePlan(const QString& wish, const QJsonArray& plan) {
    emit executionStarted();
    
    // Show plan summary
    QString summary;
    for (const QJsonValue& v : plan) {
        QString type = v.toObject()["type"].toString();
        summary += "• " + type + "\n";
    }
    
    qDebug() << "Execution plan for" << wish << ":\n" << summary;
    emit planGenerated(summary);
    
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    bool headless = env.value("RAWRXD_AUTO_APPROVE").compare("1", Qt::CaseInsensitive) == 0 ||
                    env.value("RAWRXD_AUTO_APPROVE").compare("true", Qt::CaseInsensitive) == 0 ||
                    env.value("CI").compare("true", Qt::CaseInsensitive) == 0 ||
                    env.contains("GITHUB_ACTIONS");
    if (!headless) {
        QMessageBox::information(nullptr, "Agent Plan", summary);
    }
    
    // Execute in background thread
    QtConcurrent::run([this, plan]() {
        SelfPatch patch;
        ReleaseAgent rel;
        MetaLearn ml;
        
        bool success = true;
        
        for (const QJsonValue& v : plan) {
            QJsonObject t = v.toObject();
            QString type = t["type"].toString();
            
            qDebug() << "Executing task:" << type;
            
            if (type == "add_kernel") {
                success = patch.addKernel(
                    t["target"].toString(), 
                    t["template"].toString()
                );
            } 
            else if (type == "add_cpp") {
                success = patch.addCpp(
                    t["target"].toString(), 
                    t["deps"].toString()
                );
            } 
            else if (type == "build") {
                QString target = t.value("target").toString();
                QStringList args = {"--build", "build", "--config", "Release"};
                if (!target.isEmpty()) {
                    args << "--target" << target;
                }
                int rc = QProcess::execute("cmake", args);
                success = (rc == 0);
            } 
            else if (type == "hot_reload") {
                success = patch.hotReload();
            } 
            else if (type == "bump_version") {
                success = rel.bumpVersion(t["part"].toString());
            } 
            else if (type == "tag") {
                success = rel.tagAndUpload();
            } 
            else if (type == "tweet") {
                success = rel.tweet(t["text"].toString());
            } 
            else if (type == "meta_learn") {
                success = ml.record(
                    t.value("quant").toString(),
                    t.value("kernel").toString(),
                    t.value("gpu").toString(),
                    t.value("tps").toDouble(),
                    t.value("ppl").toDouble()
                );
            }
            else if (type == "bench" || type == "bench_all") {
                // Benchmarks run during build
                qDebug() << "Benchmark task (handled by build system)";
            }
            else if (type == "self_test") {
                qDebug() << "Self-test task (TODO: implement test runner)";
            }
            
            if (!success) {
                qWarning() << "Task failed:" << type;
                emit executionCompleted(false);
                return;
            }
        }
        
        qDebug() << "All tasks completed successfully";
        emit executionCompleted(true);
    });
}

void AutoBootstrap::start() {
    QString wish = grabWish();
    startWithWishInternal(wish);
}
