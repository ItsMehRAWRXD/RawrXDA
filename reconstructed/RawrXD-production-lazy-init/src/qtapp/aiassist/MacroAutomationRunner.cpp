#include "MacroAutomationRunner.h"

MacroAutomationRunner::MacroAutomationRunner(QObject* parent) : QObject(parent) {}
MacroAutomationRunner::~MacroAutomationRunner() {}

bool MacroAutomationRunner::registerMacro(const Macro& macro) {
    QMutexLocker locker(&m_mutex);
    if (macro.id.isEmpty() || m_macros.contains(macro.id)) return false;
    m_macros[macro.id] = macro;
    return true;
}

bool MacroAutomationRunner::removeMacro(const QString& id) {
    QMutexLocker locker(&m_mutex);
    return m_macros.remove(id) > 0;
}

QList<MacroAutomationRunner::Macro> MacroAutomationRunner::listMacros() const {
    QMutexLocker locker(&m_mutex);
    return m_macros.values();
}

bool MacroAutomationRunner::execute(const QString& id) {
    QList<Step> steps;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_macros.contains(id)) return false;
        steps = m_macros[id].steps;
    }

    for (const Step& s : steps) {
        switch (s.type) {
        case Step::InsertText:
            emit insertTextRequested(s.arg1);
            break;
        case Step::OpenFile:
            emit openFileRequested(s.arg1);
            break;
        case Step::RunCommand: {
            QStringList args = s.arg2.split(' ', Qt::SkipEmptyParts);
            emit runCommandRequested(s.arg1, args);
            break; }
        }
    }

    emit macroCompleted(id);
    return true;
}
