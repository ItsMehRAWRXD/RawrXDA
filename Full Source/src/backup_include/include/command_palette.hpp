#pragma once

// <QWidget> removed (Qt-free build)
// <QLineEdit> removed (Qt-free build)
// <QListWidget> removed (Qt-free build)
// <QVBoxLayout> removed (Qt-free build)
// <QLabel> removed (Qt-free build)
// Qt include removed (Qt-free build)
// <QTimer> removed (Qt-free build)
// <QMap> removed (Qt-free build)
#include <functional>

/**
 * @brief VS Code/Cursor-style command palette (Ctrl+Shift+P)
 * 
 * Features:
 * - Fuzzy search for commands
 * - Recent commands tracking
 * - Category prefixes (>, @, #, :)
 * - Keyboard navigation
 * - Dark theme matching VS Code
 */
class CommandPalette  {
    /* Q_OBJECT */

public:
    struct Command {
        QString id;
        QString label;
        QString category;
        QString description;
        QKeySequence shortcut;
        std::function<void()> action;
        bool enabled = true;
    };

    explicit CommandPalette(QWidget* parent = nullptr);
    
    void registerCommand(const Command& cmd);
    void show();
    void hide();
    
signals:
    void commandExecuted(const QString& commandId);
    
protected:
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    
private slots:
    void onSearchTextChanged(const QString& text);
    void onItemActivated(QListWidgetItem* item);
    void executeSelectedCommand();
    
private:
    void setupUI();
    void updateResults(const QString& filter);
    void applyDarkTheme();
    int fuzzyMatch(const QString& pattern, const QString& text) const;
    
    QLineEdit* m_searchBox;
    QListWidget* m_resultsList;
    QLabel* m_hintLabel;
    
    QMap<QString, Command> m_commands;
    QStringList m_recentCommands;
    
    static constexpr int MAX_RESULTS = 10;
    static constexpr int MAX_RECENT = 5;
};
