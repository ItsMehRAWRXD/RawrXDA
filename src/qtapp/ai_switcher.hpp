#pragma once
#include <QMenu>
#include <QActionGroup>
#include <QInputDialog>

/**
 * @brief Cursor-style AI backend switcher menu
 * 
 * Provides runtime switching between:
 * - Local GGUF (brutal_gzip MASM inference)
 * - llama.cpp HTTP (self-hosted server)
 * - OpenAI (API key required)
 * - Claude (Anthropic API key required)
 * - Gemini (Google API key required)
 */
class AISwitcher : public QMenu {
    Q_OBJECT
public:
    explicit AISwitcher(QWidget* parent = nullptr);

signals:
    /**
     * @brief Emitted when user selects a new backend
     * @param id Backend identifier: "local" | "llama" | "openai" | "claude" | "gemini"
     * @param apiKey API key for remote backends (empty for local)
     */
    void backendChanged(QString id, QString apiKey);

private slots:
    void pickKey();

private:
    QActionGroup* m_backends{nullptr};
};
