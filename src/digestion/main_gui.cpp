// main_gui.cpp - Digestion Engine GUI entry point
// Note: DigestionGuiWidget is a native Win32 application (no Qt dependency)
#include "digestion_gui_widget.h"
#include "logging/logger.h"

int main(int argc, char *argv[]) {
    Logger logger("DigestionGUI");
    logger.info("Starting RawrXD Digestion Engine - Production Suite");

    DigestionGuiWidget widget;
    if (!widget.initialize(argc, argv)) {
        logger.error("Failed to initialize DigestionGuiWidget");
        return 1;
    }
    widget.setWindowTitle("RawrXD Digestion Engine - Production Suite");
    widget.resize(1000, 700);
    widget.show();
    
    return widget.run();
}

