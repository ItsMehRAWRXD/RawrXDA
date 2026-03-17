#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class UpdateCheckerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit UpdateCheckerWidget(QWidget* parent = nullptr);
    ~UpdateCheckerWidget();

private slots:
    void checkForUpdates();
    void onUpdateCheckFinished(QNetworkReply* reply);

private:
    QLabel* m_statusLabel;
    QLabel* m_updateInfo;
    QProgressBar* m_progressBar;
    QPushButton* m_checkButton;
    QPushButton* m_downloadButton;
    QTimer* m_checkTimer;
    QNetworkAccessManager* m_networkManager;
};
