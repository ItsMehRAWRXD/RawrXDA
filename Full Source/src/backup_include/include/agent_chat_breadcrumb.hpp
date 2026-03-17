#pragma once
/**
 * @file agent_chat_breadcrumb.hpp
 * @brief Agent chat breadcrumb widget — stub for MainWindow / Win32_IDE.
 */
// <QObject> removed (Qt-free build)
// <QStringList> removed (Qt-free build)

class AgentChatBreadcrumb  {
    /* Q_OBJECT */
public:
    explicit AgentChatBreadcrumb(QObject* parent = nullptr) : QObject(parent) {}
    QStringList modelList() const { return {}; }
    void refreshModels() {}

signals:
    void ollamaModelsUpdated();
};
