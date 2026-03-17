#include "MainWindow.h"
#include "widgets/re_pane.h"
#include "src/re/feature_flags.h"

void MainWindow::addREPane() {
    if (!REFeatureFlags::isREEnabled()) return;
    auto *rePane = new REPane(this);
    addDockWidget(Qt::RightDockWidgetArea, rePane);
}
