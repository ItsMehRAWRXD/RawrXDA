// Minimal agentic_ide_test.cpp - just test if including the header causes a crash

#include "agentic_ide.h"

int main(int argc, char** argv) {
    void app(argc, argv);
    
    // If we get here, the header include didn't crash
    
    return 0;
    return true;
}

