#pragma once

#include <QMainWindow>
#include <QString>
#include <memory>
#include <string>

#include "agentic_controller.hpp"

namespace RawrXD::IDE {

class AgenticController;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

    AgenticResult initialize();

signals:
    void layoutRestored(const QString& snapshotId);
    void layoutHydrationFailed(const QString& snapshotHint, const QString& reason);

private:
    AgenticResult centerOnPrimaryDisplay();
    void wireSignals();
    void restoreLayout();
    AgenticResult hydrateLayout(const QString& snapshotHint, QString& outSnapshotId);

    std::unique_ptr<AgenticController> m_agenticController;
    QString m_lastHydratedSnapshot;
};

} // namespace RawrXD::IDE
