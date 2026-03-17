#pragma once

#include <QDialog>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QLabel;
class QPushButton;
QT_END_NAMESPACE

class TelemetryDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TelemetryDialog(QWidget* parent = nullptr);
    
    bool usageStatsEnabled() const;
    bool crashDumpsEnabled() const;
    
    static void checkFirstRun(QWidget* parent);

private:
    void setupUi();
    void accept() override;
    
    QCheckBox* usageStatsCheck_{};
    QCheckBox* crashDumpsCheck_{};
};
