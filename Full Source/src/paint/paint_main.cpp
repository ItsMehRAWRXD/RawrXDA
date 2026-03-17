// paint_main.cpp - Paint application entry point
// Note: PaintApp is a native Win32 application (no Qt dependency)
#include "paint_app.h"
#include "logging/logger.h"

int main(int argc, char* argv[]) {
    Logger logger("PaintApp");
    logger.info("Starting RawrXD Paint application...");

    PaintApp window;
    if (!window.initialize(argc, argv)) {
        logger.error("Failed to initialize PaintApp");
        return 1;
    }
    window.show();
    return window.run();
}
