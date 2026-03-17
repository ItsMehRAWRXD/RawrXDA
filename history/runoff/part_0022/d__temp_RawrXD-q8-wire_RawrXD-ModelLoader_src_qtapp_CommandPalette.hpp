/**
 * @file CommandPalette.hpp
 * @brief VS Code-style command palette widget
 */

#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QString>
#include <QKeyEvent>

class CommandPalette : public QWidget {
    Q_OBJECT
    
public:
    explicit CommandPalette(QWidget* parent = nullptr);
    ~CommandPalette() override = default;
    
    void show();
    void hide();
    void setCommands(const QStringList& commands);
    
signals:
    void commandTriggered(const QString& command);
    void closed();
    
protected:
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    
private slots:
    void onSearchTextChanged(const QString& text);
    void onItemActivated(QListWidgetItem* item);
    
private:
    void setupUI();
    void filterCommands(const QString& filter);
    
    QLineEdit* m_searchEdit{};
    QListWidget* m_commandList{};
    QLabel* m_headerLabel{};
    QStringList m_allCommands;
};
