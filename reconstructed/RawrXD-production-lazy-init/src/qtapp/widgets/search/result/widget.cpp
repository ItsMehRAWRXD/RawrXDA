/**
 * @file search_result_widget.cpp
 * @brief Implementation of SearchResultWidget - Search results
 */

#include "search_result_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>

SearchResultWidget::SearchResultWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    setWindowTitle("Search Results");
}

SearchResultWidget::~SearchResultWidget() = default;

void SearchResultWidget::setupUI()
{
    mMainLayout = new QVBoxLayout(this);
    
    QHBoxLayout* searchLayout = new QHBoxLayout();
    mSearchInput = new QLineEdit(this);
    mSearchInput->setPlaceholderText("Search...");
    searchLayout->addWidget(mSearchInput);
    
    mSearchButton = new QPushButton("Search", this);
    mSearchButton->setStyleSheet("background-color: #2196F3; color: white;");
    searchLayout->addWidget(mSearchButton);
    
    mClearButton = new QPushButton("Clear", this);
    searchLayout->addWidget(mClearButton);
    
    mExportButton = new QPushButton("Export", this);
    searchLayout->addWidget(mExportButton);
    
    mMainLayout->addLayout(searchLayout);
    
    mResultsTree = new QTreeWidget(this);
    mResultsTree->setHeaderLabels({"File", "Line", "Match"});
    mMainLayout->addWidget(mResultsTree);
}

void SearchResultWidget::onSearch() {}
void SearchResultWidget::onClear() {}
void SearchResultWidget::onExportResults() {}
