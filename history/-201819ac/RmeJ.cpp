#include "HexMagConsole.h"

HexMagConsole::HexMagConsole(QWidget* parent) : QPlainTextEdit(parent) {
    setReadOnly(true);
    setStyleSheet("background-color: #1e1e1e; color: #d4d4d4; font-family: Consolas, monospace;");
}

HexMagConsole::~HexMagConsole() {}

void HexMagConsole::appendLog(const QString& message) {
    appendPlainText(message);
}

void HexMagConsole::displayHex(const QByteArray& data) {
    QString hex = data.toHex(' ').toUpper();
    appendPlainText(hex);
}
