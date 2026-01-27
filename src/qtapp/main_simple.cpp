#include "MainWindowSimple.h"
#include <iostream>

int main(int argc, char* argv[])
{
    std::cout << "Starting RawrXD Simple IDE..." << std::endl;
    MainWindow window;
    window.show();
    std::cout << "IDE initialized with advanced features." << std::endl;
    return window.exec();
}