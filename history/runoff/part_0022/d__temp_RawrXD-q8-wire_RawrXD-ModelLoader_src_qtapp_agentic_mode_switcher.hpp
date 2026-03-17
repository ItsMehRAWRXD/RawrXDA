#pragma once
#include <QWidget>
#include <QComboBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QTimer>

/**
 * @brief Agentic Mode Switcher - Cursor/GitHub Copilot style mode selector
 * 
 * Three operational modes:
 * - Plan Mode: Research + Planning (runSubagent → present plan → await approval)
 * - Agent Mode: Autonomous execution (manage_todo_list + runSubagent)
 * - Ask Mode: Simple Q&A with verification
 */
class AgenticModeSwitcher : public QWidget {
    Q_OBJECT

public:
    enum Mode {
        ASK_MODE = 0,      // Simple question answering with verification
        PLAN_MODE = 1,     // Planning with subagent research
        AGENT_MODE = 2     // Autonomous execution
    };

    explicit AgenticModeSwitcher(QWidget* parent = nullptr);
    
    Mode currentMode() const { return m_currentMode; }
    void setMode(Mode mode);
    
    // Visual feedback for mode activity
    void setModeActive(bool active);
    void showProgress(const QString& message);
    
signals:
    void modeChanged(Mode newMode);
    void modeActivated(Mode mode, const QString& input);
    
private slots:
    void onModeSelected(int index);
    
private:
    void setupUI();
    void applyDarkTheme();
    void updateModeDescription();
    
    QComboBox* m_modeSelector;
    QLabel* m_modeIcon;
    QLabel* m_modeDescription;
    QLabel* m_activityIndicator;
    QTimer* m_activityTimer;
    
    Mode m_currentMode;
    bool m_isActive;
    int m_activityFrame;
};
