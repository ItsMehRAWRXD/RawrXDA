#pragma once

#include <QWidget>
#include <QString>
#include <vector>
#include <cstdint>

class QTabWidget;
class QTextEdit;
class QLineEdit;
class QProcess;

struct TerminalInfo {
    QTextEdit* output_widget;
    QLineEdit* input_widget;
    QProcess* process;
};

class TerminalPool : public QWidget {
    Q_OBJECT
public:
    explicit TerminalPool(uint32_t pool_size, QWidget* parent = nullptr);
    void initialize();
    
public slots:
    void createNewTerminal();
    void executeCommand(int terminal_index);
    void readProcessOutput(int terminal_index);
    void readProcessError(int terminal_index);
    void closeTerminal(int tab_index);
    
signals:
    void commandExecuted(const QString& command);
    
private:
    uint32_t pool_size_;
    QTabWidget* tab_widget_;
    std::vector<TerminalInfo> terminals_;
};
