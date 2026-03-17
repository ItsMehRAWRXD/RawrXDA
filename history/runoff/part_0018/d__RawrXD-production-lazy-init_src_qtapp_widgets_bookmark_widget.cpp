/**
 * @file bookmark_widget.cpp
 * @brief Implementation of BookmarkWidget - Code bookmarks
 */

#include "bookmark_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QListWidget>

BookmarkWidget::BookmarkWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    setWindowTitle("Bookmarks");
}

BookmarkWidget::~BookmarkWidget() = default;

void BookmarkWidget::setupUI()
{
    mMainLayout = new QVBoxLayout(this);
    
    QHBoxLayout* inputLayout = new QHBoxLayout();
    mBookmarkInput = new QLineEdit(this);
    mBookmarkInput->setPlaceholderText("Bookmark name...");
    inputLayout->addWidget(mBookmarkInput);
    
    mAddButton = new QPushButton("Add", this);
    mAddButton->setStyleSheet("background-color: #4CAF50; color: white;");
    inputLayout->addWidget(mAddButton);
    
    mMainLayout->addLayout(inputLayout);
    
    mBookmarkList = new QListWidget(this);
    mMainLayout->addWidget(mBookmarkList);
    
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    mGoToButton = new QPushButton("Go To", this);
    buttonLayout->addWidget(mGoToButton);
    
    mRemoveButton = new QPushButton("Remove", this);
    buttonLayout->addWidget(mRemoveButton);
    
    buttonLayout->addStretch();
    mMainLayout->addLayout(buttonLayout);
}

void BookmarkWidget::onAddBookmark() {}
void BookmarkWidget::onRemoveBookmark() {}
void BookmarkWidget::onGoToBookmark() {}
