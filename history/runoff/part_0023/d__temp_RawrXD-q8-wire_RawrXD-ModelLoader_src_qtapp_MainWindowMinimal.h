#pragma once
#include <QMainWindow>

class MainWindowMinimal : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindowMinimal(QWidget *parent = nullptr);
    ~MainWindowMinimal();

private slots:
    void newFile();
    void openFile();
};