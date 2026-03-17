#include "HexMagConsole.h"
#include <QTime>

HexMagConsole::HexMagConsole(QWidget *parent) : QPlainTextEdit(parent) {
    setReadOnly(true);
    setStyleSheet("background-color: #1e1e1e; color: #00ff00; font-family: Consolas, monospace;");
}

void HexMagConsole::appendLog(const QString &message) {
    QString timestamp = QTime::currentTime().toString("HH:mm:ss");
    appendPlainText(QString("[%1] %2").arg(timestamp, message));
}

void HexMagConsole::displayHex(const QByteArray &data) {
    QString hexOutput;
    const int bytesPerLine = 16;
    for (int i = 0; i < data.size(); i += bytesPerLine) {
        QByteArray line = data.mid(i, bytesPerLine);
        QString hex = line.toHex(' ').toUpper();
        QString ascii;
        for (char c : line) {
            if (c >= 32 && c <= 126) {
                ascii.append(c);
            } else {
                ascii.append('.');
            }
        }
        hexOutput.append(QString("%1  %2  |%3|\n")
                         .arg(i, 4, 16, QChar('0'))
                         .arg(hex, -48)
                         .arg(ascii));
    }
    appendPlainText(hexOutput);
}
