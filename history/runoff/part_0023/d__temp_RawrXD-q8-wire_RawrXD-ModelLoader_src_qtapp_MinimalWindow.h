#pragma once
#include <QMainWindow>

class MinimalWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MinimalWindow(QWidget *parent = nullptr);
    ~MinimalWindow();
};