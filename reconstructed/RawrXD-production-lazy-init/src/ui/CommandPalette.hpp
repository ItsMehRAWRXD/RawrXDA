// src/ui/CommandPalette.hpp
// Cursor-class command palette with fuzzy matching, icons, and enterprise features

#pragma once

#include <QWidget>
#include <QPointer>
#include <QKeySequence>
#include <memory>
#include <functional>
#include <vector>

QT_BEGIN_NAMESPACE
class QListWidget;
class QLineEdit;
class QLabel;
class QKeyEvent;
class QShowEvent;
QT_END_NAMESPACE

struct CommandPaletteItem {
    QString id;
    QString label;          // user visible
    QString category;       // File / Edit / AI / Git …
    QKeySequence keySeq;    // Ctrl+P, Ctrl+Shift+P …
    QString iconName;       // from Qt resource :/icons/...
    std::function<void(const QString& arg)> handler; // arg = fuzzy filter
    int score = 0;          // fuzzy score (higher = better)
};

class CommandPalette : public QWidget {
    Q_OBJECT
public:
    explicit CommandPalette(QWidget* parent = nullptr);
    void showAt(const QPoint& globalPos); // slide-down animation
    void reloadCommands();                // refresh after plugin load

    // === Dynamic command registration (used by all subsystems) ===
    void registerCommand(const QString& id, const QString& label,
                         const QString& category, const QKeySequence& key,
                         const QString& icon,
                         std::function<void(const QString&)> handler);

    void registerCommand(const QString& id, const QString& label,
                         const QString& category,
                         std::function<void(const QString&)> handler);

    // Bulk registration helper
    void registerCommands(const std::vector<CommandPaletteItem>& commands);

    // Remove commands by category or id prefix
    void unregisterCategory(const QString& category);
    void unregisterCommand(const QString& id);

    // Query
    int commandCount() const { return static_cast<int>(m_items.size()); }
    bool hasCommand(const QString& id) const;

    // Singleton access for global registration
    static CommandPalette* instance();

signals:
    void closed();
    void commandExecuted(const QString& commandId);

protected:
    void keyPressEvent(QKeyEvent* e) override;
    void showEvent(QShowEvent* e) override;

private:
    void buildModel();      // fill m_items
    void filter(const QString& text);
    void execSelected();
    int  fuzzyScore(const QString& pattern, const QString& text) const;

    QLineEdit* m_input;
    QListWidget* m_list;
    QLabel* m_preview;      // inline peek label
    std::vector<CommandPaletteItem> m_items;
    std::vector<CommandPaletteItem*> m_visible;

    static CommandPalette* s_instance;
};
