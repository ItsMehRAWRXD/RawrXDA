#pragma once
#include <QMainWindow>

class QShowEvent;
class MultiTabEditor;
class ChatInterface;
class TerminalPool;
class AgenticEngine;

class AgenticIDE : public QMainWindow {
public:
    explicit AgenticIDE(QWidget *parent = nullptr);
    ~AgenticIDE();

protected:
    void showEvent(QShowEvent *ev) override;

private:
    // Core UI components
    MultiTabEditor *m_multiTabEditor = nullptr;
    ChatInterface *m_chatInterface = nullptr;
    TerminalPool *m_terminalPool = nullptr;
    class QDockWidget *m_chatDock = nullptr;
    class QDockWidget *m_terminalDock = nullptr;
    
    // Agent engine
    AgenticEngine *m_agenticEngine = nullptr;
};
