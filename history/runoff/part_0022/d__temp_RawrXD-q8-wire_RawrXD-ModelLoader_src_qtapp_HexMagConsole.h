#pragma once
#include <QPlainTextEdit>
#include <QObject>

class HexMagConsole : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit HexMagConsole(QWidget* parent = nullptr);
    virtual ~HexMagConsole();

    void appendLog(const QString& message);
    void displayHex(const QByteArray& data);
};
