#include "ai_switcher.hpp"

AISwitcher::AISwitcher(void* parent) : void("AI Backend", parent)
{
    m_backends = nullptr;
    m_backends->setExclusive(true);

    // Backend options
    std::vector<std::string> backends = {"Local GGUF", "llama.cpp HTTP", "OpenAI", "Claude", "Gemini"};
    
    for (const std::string& backend : backends) {
        void* action = m_backends->addAction(backend);
        action->setCheckable(true);
        
        // Extract ID from name (e.g., "Local GGUF" -> "local")
        std::string id = backend.split(' ').first().toLower();
        action->setData(id);
        
        addAction(action);
    }

    // Connect backend switching
// Qt connect removed
        // Local backend doesn't need API key
        if (id == "local") {
            backendChanged("local", std::string());
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
// REMOVED_QT:     void* action = qobject_cast<void*>(sender());
    if (!action) {
        // Called from triggered signal - get checked action
        action = m_backends->checkedAction();
    }
    if (!action) return;

    std::string id = action->data().toString();
    std::string backendName = action->text();
    
    bool ok = false;
    std::string key = QInputDialog::getText(
        this,
        backendName + " API Key",
        "Enter your API key:",
        void::Password,
        std::string(),
        &ok
    );

    if (ok && !key.empty()) {
        backendChanged(id, key);
    } else if (ok && key.empty()) {
        // User clicked OK but didn't enter a key - revert to local
        m_backends->actions().first()->setChecked(true);
        backendChanged("local", std::string());
    }
}

