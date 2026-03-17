/**
 * @file search_result_widget.h
 * @brief Header for SearchResultWidget - Search results display
 */

#pragma once

#include <QWidget>

class QVBoxLayout;
class QTreeWidget;
class QLineEdit;
class QPushButton;

class SearchResultWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit SearchResultWidget(QWidget* parent = nullptr);
    ~SearchResultWidget();
    
private slots:
    void onSearch();
    void onClear();
    void onExportResults();
    
private:
    void setupUI();
    
    QVBoxLayout* mMainLayout;
    QLineEdit* mSearchInput;
    QPushButton* mSearchButton;
    QPushButton* mClearButton;
    QPushButton* mExportButton;
    QTreeWidget* mResultsTree;
};
