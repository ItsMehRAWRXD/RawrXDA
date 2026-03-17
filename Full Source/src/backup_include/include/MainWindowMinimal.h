#pragma once
// <QMainWindow> removed (Qt-free build)

class MainWindowMinimal  {
    /* Q_OBJECT */

public:
    explicit MainWindowMinimal(QWidget *parent = nullptr);
    ~MainWindowMinimal();

private slots:
    void newFile();
    void openFile();
};