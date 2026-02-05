#pragma once

#include <QObject>
#include <QString>

class ZeroTouch : public QObject {
    Q_OBJECT
public:
    explicit ZeroTouch(QObject* parent = nullptr);

    void installAll();
    void installFileWatcher();
    void installGitHook();
    void installVoiceTrigger();

private:
    QString m_lastVoiceWish;
};
