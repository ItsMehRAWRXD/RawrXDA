#include "paint/paint_app.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    PaintApp window;
    window.show();
    return app.exec();
}
