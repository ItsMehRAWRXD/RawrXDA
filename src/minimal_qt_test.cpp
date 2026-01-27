#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <cstdio>

int main(int argc, char* argv[]) {
    std::fprintf(stderr, "TEST: Before QApplication\n");
    std::fflush(stderr);
    
    QApplication app(argc, argv);
    
    std::fprintf(stderr, "TEST: QApplication created\n");
    std::fflush(stderr);
    
    QMainWindow window;
    window.setCentralWidget(new QLabel("Hello Qt!"));
    window.show();
    
    std::fprintf(stderr, "TEST: Window shown\n");
    std::fflush(stderr);
    
    return app.exec();
}
