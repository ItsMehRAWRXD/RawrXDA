#include "digestion_gui_widget.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    DigestionGuiWidget widget;
    widget.setWindowTitle("RawrXD Digestion Engine - Production Suite");
    widget.resize(1000, 700);
    widget.show();
    
    return app.exec();
}

