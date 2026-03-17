#include "re_pane.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include "../../re/feature_flags.h"
#include "../../re/causal_graph.h"
#include "../../re/re_manager.h"

REPane::REPane(REManager *manager, QWidget *parent) : QWidget(parent), m_manager(manager) {
    QVBoxLayout *layout = new QVBoxLayout(this);
    QLabel *title = new QLabel("Reverse Engineering Pane", this);
    layout->addWidget(title);
    if (!REFeatureFlags::isREEnabled()) {
        layout->addWidget(new QLabel("RE features are disabled. Set RAWRXD_RE_ENABLED=1 to enable.", this));
        setLayout(layout);
        return;
    }
    // Add UI for binary load, disasm, decomp, dynamic attach
    setLayout(layout);
}

void REPane::setBinary(const QString &path) {
    // TODO: Integrate with REManager
}

void REPane::setProcess(quint64 pid) {
    // TODO: Integrate with REManager
}

// Example: Visualize causal graph stats in REPane
void REPane::showCausalGraphStats() {
    if (!m_manager) return;
    auto &graph = m_manager->causalGraph();
    int entities = graph.entityCount();
    int links = graph.linkCount();
    int rev = graph.revision();
    // Display in UI
    QLabel *statsLabel = new QLabel(QString("Causal Graph: %1 entities, %2 links, rev %3").arg(entities).arg(links).arg(rev), this);
    QVBoxLayout *layout = qobject_cast<QVBoxLayout *>(this->layout());
    if (layout) layout->addWidget(statsLabel);
}
