#pragma once
#include <QWidget>
#include <QString>
#include <QHash>
#include <QVBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>

class AICompletionCache : public QWidget {
    Q_OBJECT
public:
    explicit AICompletionCache(QWidget* parent = nullptr);
    ~AICompletionCache();

    void setCache(const QString& key, const QString& value, int ttlMs = 300000);
    QString getCache(const QString& key) const;
    void clearCache();

private slots:
    void onClearCache();
    void updateStats();

private:
    void setupUI();
    QHash<QString, QPair<QString, qint64>> m_cache;
    QLabel* m_statsLabel;
    QPlainTextEdit* m_cacheDisplay;
    QPushButton* m_clearButton;
};