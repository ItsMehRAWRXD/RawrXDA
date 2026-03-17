#pragma once
#include <QMainWindow>
#include "HexMagConsole.h"
#include "unified_hotpatch_manager.hpp"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_hotpatchButton_clicked();

private:
    Ui::MainWindow *ui;
    HexMagConsole *m_hexMagConsole;
    UnifiedHotpatchManager *m_hotpatchManager;
};
