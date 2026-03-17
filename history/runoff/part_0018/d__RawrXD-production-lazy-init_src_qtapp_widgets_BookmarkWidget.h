/**
 * @file BookmarkWidget.h
 * @brief Header for BookmarkWidget - Complete implementation
 */

#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>

namespace RawrXD {

class BookmarkWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BookmarkWidget(QWidget* parent = nullptr);
    ~BookmarkWidget();

    void initialize();
    void refresh();

public slots:
    void onUpdate();

private:
    void setupUI();

    // UI components
    QVBoxLayout* mMainLayout;
    QLabel* mTitleLabel;
};

} // namespace RawrXD

// Global typedef for MainWindow.h compatibility
using BookmarkWidget = RawrXD::BookmarkWidget;

