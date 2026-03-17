// Diagnostic Panel - Real-time error display
#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QPushButton>
#include <QTimer>
#include "diagnostic_logger.h"

namespace RawrXD {

class DiagnosticPanel : public QWidget {
    Q_OBJECT

public:
    explicit DiagnosticPanel(QWidget* parent = nullptr);
    
public slots:
    void refreshLogs();
    void clearLogs();
    void copyLogs();
    
private:
    QVBoxLayout* m_layout;
    QTextEdit* m_logDisplay;
    QPushButton* m_refreshBtn;
    QPushButton* m_clearBtn;
    QPushButton* m_copyBtn;
    QTimer* m_refreshTimer;
};

} // namespace RawrXD