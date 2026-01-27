#include "rollback.hpp"
#include "meta_learn.hpp"
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QDebug>

// ---------- 1. detect regression ----------
bool Rollback::detectRegression() {
    // load before/after from perf_db.json via MetaLearn
    bool ok = false;
    QJsonArray db = MetaLearn::loadDB(&ok);
    if (!ok) {
        qWarning() << "Rollback: unable to read perf_db.json";
        return false;
    }
    if (db.size() < 2)
        return false; // need at least 2 records

    // last commit = most recent, previous = second-most recent
    QJsonObject last = db.last().toObject();
    QJsonObject prev = db.at(db.size() - 2).toObject();

    double lastTPS = last.value("tps").toDouble();
    double prevTPS = prev.value("tps").toDouble();
    double lastPPL = last.value("ppl").toDouble();
    double prevPPL = prev.value("ppl").toDouble();

    // regression: TPS drop > 5 % OR PPL increase > 2 %
    bool tpsReg = lastTPS < prevTPS * 0.95;
    bool pplReg = lastPPL > prevPPL * 1.02;

    qInfo() << "Rollback::detectRegression" << "tpsReg=" << tpsReg << "pplReg=" << pplReg
            << "lastTPS=" << lastTPS << "prevTPS=" << prevTPS
            << "lastPPL=" << lastPPL << "prevPPL=" << prevPPL;

    return tpsReg || pplReg;
}

// ---------- 2. git revert ----------
bool Rollback::revertLastCommit() {
    QProcess proc;
    proc.start("git", {"revert", "--no-edit", "HEAD"});
    if (!proc.waitForFinished(60000)) {
        qWarning() << "Rollback: git revert timed out";
        return false;
    }
    if (proc.exitCode() != 0) {
        qWarning() << "Rollback: git revert failed" << proc.readAllStandardError();
        return false;
    }
    qInfo() << "Rollback: git revert SUCCESS";
    return true;
}

// ---------- 3. open GitHub issue ----------
bool Rollback::openIssue(const QString& title, const QString& body) {
    QString token = qEnvironmentVariable("GITHUB_TOKEN");
    if (token.isEmpty()) {
        qWarning() << "Rollback: GITHUB_TOKEN not set  skipping issue";
        return true; // allow in dev
    }

    QJsonObject issue{
        {"title", title},
        {"body", body},
        {"labels", QJsonArray{QStringLiteral("regression"), QStringLiteral("auto")}}
    };

    QNetworkRequest req(QUrl("https://api.github.com/repos/ItsMehRAWRXD/RawrXD-ModelLoader/issues"));
    req.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkAccessManager nam;
    QNetworkReply* reply = nam.post(req, QJsonDocument(issue).toJson());
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    bool ok = reply->error() == QNetworkReply::NoError;
    if (!ok) {
        qWarning() << "Rollback: GitHub issue failed" << reply->errorString();
    } else {
        qInfo() << "Rollback: GitHub issue opened" << title;
    }
    reply->deleteLater();
    return ok;
}
