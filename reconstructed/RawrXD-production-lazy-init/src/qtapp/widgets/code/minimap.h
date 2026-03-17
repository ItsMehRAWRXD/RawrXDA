/**
 * @file code_minimap.h
 * @brief Header for CodeMinimap - Code minimap visualization
 */

#pragma once

#include <QWidget>

class QVBoxLayout;
class QLabel;

class CodeMinimap : public QWidget {
    Q_OBJECT
    
public:
    explicit CodeMinimap(QWidget* parent = nullptr);
    ~CodeMinimap();
    
public slots:
    void onCodeUpdated(const QString& code);
    void onPositionChanged(int line);
    
signals:
    void lineSelected(int lineNumber);
    
private:
    void setupUI();
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    
    QVBoxLayout* mMainLayout;
    QLabel* mLabel;
    QString mCodeContent;
    int mCurrentLine;
};

#endif // CODE_MINIMAP_H
