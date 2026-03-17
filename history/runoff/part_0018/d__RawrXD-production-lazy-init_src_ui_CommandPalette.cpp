// src/ui/CommandPalette.cpp
// Cursor-class command palette implementation with fuzzy matching and enterprise features

#include "CommandPalette.hpp"
#include <QVBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLabel>
#include <QKeyEvent>
#include <QTimer>
#include <QPropertyAnimation>
#include <QIcon>
#include <QFileDialog>
#include <QInputDialog>
#include <QCursor>
#include <QDebug>
#include <algorithm>

// Singleton instance
CommandPalette* CommandPalette::s_instance = nullptr;

CommandPalette* CommandPalette::instance() {
    return s_instance;
}

CommandPalette::CommandPalette(QWidget* parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint)
{
    s_instance = this;
    setAttribute(Qt::WA_TranslucentBackground);
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(4,4,4,4);

    m_input = new QLineEdit(this);
    m_input->setPlaceholderText("What do you want to do?");
    m_input->setClearButtonEnabled(true);
    lay->addWidget(m_input);

    m_list = new QListWidget(this);
    m_list->setUniformItemSizes(true);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    lay->addWidget(m_list);

    m_preview = new QLabel(this);
    m_preview->setVisible(false);
    lay->addWidget(m_preview);

    connect(m_input, &QLineEdit::textChanged, this, &CommandPalette::filter);
    connect(m_list, &QListWidget::itemActivated, this, &CommandPalette::execSelected);

    buildModel();
    filter({}); // show initial
}

void CommandPalette::buildModel() {
    m_items.clear();
    auto add = [&](const QString& id, const QString& label,
                   const QString& cat, const QKeySequence& key,
                   const QString& icon, auto handler) {
        m_items.push_back({id, label, cat, key, icon, handler});
    };
    
    // ---------- AI section ----------
    add("ai.generate", "AI: Generate function", "AI",
        QKeySequence("Ctrl+Alt+G"), ":/icons/ai.png",
        [](const QString& arg){ 
            // Stub - would call AIChat::instance()->generate(arg)
            qDebug() << "AI Generate triggered with:" << arg;
        });

    add("ai.rewrite", "AI: Rewrite selection", "AI",
        QKeySequence("Ctrl+Alt+R"), ":/icons/ai.png",
        [](const QString&){ 
            // Stub - would call SmartRewriteEngine::instance()->trigger()
            qDebug() << "AI Rewrite triggered";
        });

    add("ai.chat", "AI: Open chat panel", "AI",
        QKeySequence("Ctrl+Shift+C"), ":/icons/chat.png",
        [](const QString&){ 
            // Stub - would call AgenticNavigator::instance()->showChat()
            qDebug() << "AI Chat triggered";
        });

    // ---------- File ----------
    add("file.open", "File: Open...", "File",
        QKeySequence("Ctrl+O"), ":/icons/file.png",
        [](const QString& filter){
            QString f = QFileDialog::getOpenFileName(nullptr, "Open", {}, filter);
            if(!f.isEmpty()) {
                // Stub - would call Editor::open(f)
                qDebug() << "File Open triggered:" << f;
            }
        });

    add("file.save", "File: Save", "File",
        QKeySequence("Ctrl+S"), ":/icons/save.png",
        [](const QString&){ 
            // Stub - would call Editor::saveCurrent()
            qDebug() << "File Save triggered";
        });

    // ---------- Git ----------
    add("git.checkout", "Git: Checkout branch...", "Git",
        QKeySequence("Ctrl+Shift+B"), ":/icons/git.png",
        [](const QString& arg){
            // Stub - would call Git::branches() and Git::checkout()
            qDebug() << "Git Checkout triggered with:" << arg;
        });

    // ---------- Recent ----------
    // Stub - would populate from RecentCommands::instance()->items()
    add("recent.test", "Recent: Test Command", "Recent", 
        QKeySequence(), ":/icons/recent.png",
        [](const QString&){ 
            qDebug() << "Recent Command triggered";
        });
}

void CommandPalette::filter(const QString& text) {
    m_visible.clear();
    for(auto& item : m_items) {
        item.score = fuzzyScore(text, item.label);
        if(item.score > 0 || text.isEmpty())
            m_visible.push_back(&item);
    }
    std::sort(m_visible.begin(), m_visible.end(),
              [](auto* a, auto* b){ return a->score > b->score; });

    m_list->clear();
    for(auto* it : m_visible) {
        auto* wi = new QListWidgetItem(m_list);
        wi->setText(it->label);
        
        // Use default Qt icons for now - can be replaced with custom icons
        if (it->category == "AI") {
            wi->setIcon(QIcon::fromTheme("dialog-information"));
        } else if (it->category == "File") {
            wi->setIcon(QIcon::fromTheme("document-open"));
        } else if (it->category == "Git") {
            wi->setIcon(QIcon::fromTheme("vcs-branch"));
        } else {
            wi->setIcon(QIcon::fromTheme("view-history"));
        }
        
        if(!it->keySeq.isEmpty())
            wi->setText(it->label + "\t" + it->keySeq.toString());
        wi->setData(Qt::UserRole, QVariant::fromValue(it));
    }
    if(m_list->count()) m_list->setCurrentRow(0);
}

int CommandPalette::fuzzyScore(const QString& pattern, const QString& text) const {
    if(pattern.isEmpty()) return 1;
    QString p = pattern.toLower(), t = text.toLower();
    int score = 0, j = 0;
    for(int i = 0; i < p.size() && j < t.size(); ++i) {
        while(j < t.size() && t[j] != p[i]) ++j;
        if(j < t.size()) score += (j+1)*10;
    }
    return score;
}

void CommandPalette::execSelected() {
    auto* wi = m_list->currentItem();
    if(!wi) return;
    auto* item = wi->data(Qt::UserRole).value<CommandPaletteItem*>();
    
    // Telemetry integration - would call TelemetryManager::instance()->increment()
    qDebug() << "[Telemetry] command_palette.exec{command=" << item->id << "}";
    
    emit commandExecuted(item->id);
    item->handler(m_input->text()); // pass filter as arg
    close();
}

void CommandPalette::showAt(const QPoint& globalPos) {
    resize(600, 400);
    move(globalPos - QPoint(0, height()/2));
    show();
    raise();
    m_input->setFocus();
    
    // slide-down animation
    auto* anim = new QPropertyAnimation(this, "pos");
    anim->setStartValue(pos() - QPoint(0, 20));
    anim->setEndValue(pos());
    anim->setDuration(120);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void CommandPalette::keyPressEvent(QKeyEvent* e) {
    if(e->key() == Qt::Key_Escape) { close(); return; }
    if(e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        execSelected(); return;
    }
    if(e->key() == Qt::Key_Down)  { 
        m_list->setCurrentRow(std::min(m_list->currentRow()+1, m_list->count()-1)); 
        return; 
    }
    if(e->key() == Qt::Key_Up)    { 
        m_list->setCurrentRow(std::max(m_list->currentRow()-1, 0)); 
        return; 
    }
    QWidget::keyPressEvent(e);
}

void CommandPalette::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    m_input->setFocus();
}

void CommandPalette::reloadCommands() {
    buildModel();
    filter(m_input->text());
}

// === Dynamic command registration ===

void CommandPalette::registerCommand(const QString& id, const QString& label,
                                     const QString& category, const QKeySequence& key,
                                     const QString& icon,
                                     std::function<void(const QString&)> handler) {
    // Prevent duplicates
    unregisterCommand(id);
    m_items.push_back({id, label, category, key, icon, handler, 0});
    filter(m_input ? m_input->text() : QString());
}

void CommandPalette::registerCommand(const QString& id, const QString& label,
                                     const QString& category,
                                     std::function<void(const QString&)> handler) {
    registerCommand(id, label, category, QKeySequence(), QString(), handler);
}

void CommandPalette::registerCommands(const std::vector<CommandPaletteItem>& commands) {
    for (const auto& cmd : commands) {
        unregisterCommand(cmd.id);
        m_items.push_back(cmd);
    }
    filter(m_input ? m_input->text() : QString());
}

void CommandPalette::unregisterCategory(const QString& category) {
    m_items.erase(
        std::remove_if(m_items.begin(), m_items.end(),
                       [&](const CommandPaletteItem& item) { return item.category == category; }),
        m_items.end());
}

void CommandPalette::unregisterCommand(const QString& id) {
    m_items.erase(
        std::remove_if(m_items.begin(), m_items.end(),
                       [&](const CommandPaletteItem& item) { return item.id == id; }),
        m_items.end());
}

bool CommandPalette::hasCommand(const QString& id) const {
    return std::any_of(m_items.begin(), m_items.end(),
                       [&](const CommandPaletteItem& item) { return item.id == id; });
}
