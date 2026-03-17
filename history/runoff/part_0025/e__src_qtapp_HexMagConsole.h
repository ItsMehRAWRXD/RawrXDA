#pragma once
#include <QPlainTextEdit>
#include <QString>
#include <QByteArray>

class HexMagConsole : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit HexMagConsole(QWidget *parent = nullptr);
    void appendLog(const QString &message);
    void displayHex(const QByteArray &data);
};
