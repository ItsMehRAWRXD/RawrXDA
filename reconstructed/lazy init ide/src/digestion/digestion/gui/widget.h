// digestion_gui_widget.h
#pragma once
#include <QWidget>
#include <QProgressBar>
#include <QTableWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include "digestion_reverse_engineering.h"

class DigestionGuiWidget : public QWidget {
    Q_OBJECT
public:
    explicit DigestionGuiWidget(QWidget *parent = nullptr);
    void setRootDirectory(const QString &path);

private slots:
    void startDigestion();
    void stopDigestion();
    void onProgress(int done, int total, int stubs, int percent);
    void onFileScanned(const QString &path, const QString &lang, int stubs);
    void onFinished(const QJsonObject &report, qint64 elapsed);
    void browseDirectory();

private:
    DigestionReverseEngineeringSystem *m_digester;
    QLineEdit *m_pathEdit;
    QProgressBar *m_progressBar;
    QTableWidget *m_resultsTable;
    QPushButton *m_startBtn;
    QPushButton *m_stopBtn;
    QCheckBox *m_applyFixesCheck;
    QCheckBox *m_gitModeCheck;
    QCheckBox *m_incrementalCheck;
};
