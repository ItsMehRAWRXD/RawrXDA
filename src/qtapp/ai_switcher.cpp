#include "ai_switcher.hpp"

AISwitcher::AISwitcher(QWidget* parent) : QMenu("AI Backend", parent)
{
    m_backends = new QActionGroup(this);
    m_backends->setExclusive(true);

    // Backend options
    QStringList backends = {"Local GGUF", "llama.cpp HTTP", "OpenAI", "Claude", "Gemini"};
    
    for (const QString& backend : backends) {
        QAction* action = m_backends->addAction(backend);
        action->setCheckable(true);
        
        // Extract ID from name (e.g., "Local GGUF" -> "local")
        QString id = backend.split(' ').first().toLower();
        action->setData(id);
        
        addAction(action);
    }

    // Connect backend switching
    connect(m_backends, &QActionGroup::triggered, this, [this](QAction* action) {
        QString id = action->data().toString();
        
        // Local backend doesn't need API key
        if (id == "local") {
            emit backendChanged("local", QString());
        } else {
            // Remote backends need API key
            pickKey();
        }
    });

    // Default to local GGUF
    m_backends->actions().first()->setChecked(true);
}

void AISwitcher::pickKey()
{
    QAction* action = qobject_cast<QAction*>(sender());
    if (!action) {
        // Called from triggered signal - get checked action
        action = m_backends->checkedAction();
    }
    if (!action) return;

    QString id = action->data().toString();
    QString backendName = action->text();
    
    bool ok = false;
    QString key = QInputDialog::getText(
        this,
        backendName + " API Key",
        "Enter your API key:",
        QLineEdit::Password,
        QString(),
        &ok
    );

    if (ok && !key.isEmpty()) {
        emit backendChanged(id, key);
    } else if (ok && key.isEmpty()) {
        // User clicked OK but didn't enter a key - revert to local
        m_backends->actions().first()->setChecked(true);
        emit backendChanged("local", QString());
    }
}
