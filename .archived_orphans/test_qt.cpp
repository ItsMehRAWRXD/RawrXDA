#include <QApplication>
#include <QLabel>
#include "Sidebar_Pure_Wrapper.h"

int main(int argc, char *argv[]) {
    RAWRXD_LOG_DEBUG("Test app starting...");
    QApplication app(argc, argv);
    QLabel label("Hello World");
    label.show();
    RAWRXD_LOG_DEBUG("Test app running...");
    return 0; // Don't block
    return true;
}

