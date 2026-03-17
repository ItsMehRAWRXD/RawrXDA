#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QStatusBar>
#include <QDateTime>
#include <QFile>
#include <QTextStream>

void appendLifecycleLog(const QString& line) {
    QFile f("terminal_diagnostics.log");
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz")
           << " " << line << "\n";
    }
}

class MinimalMainWindow : public QMainWindow {
public:
    explicit MinimalMainWindow(QWidget* parent = nullptr) : QMainWindow(parent) {
        appendLifecycleLog("MinimalMainWindow constructor start");
        setWindowTitle("Minimal Test");
        resize(800, 600);
        
        QWidget* central = new QWidget(this);
        QVBoxLayout* layout = new QVBoxLayout(central);
        QLabel* label = new QLabel("Minimal Test Window", central);
        layout->addWidget(label);
        setCentralWidget(central);
        setStatusBar(new QStatusBar(this));
        statusBar()->showMessage("Minimal test running");
        
        appendLifecycleLog("MinimalMainWindow constructor complete");
    }
};

int main(int argc, char* argv[]) {
    appendLifecycleLog("[TEST] main() start");
    
    QApplication app(argc, argv);
    appendLifecycleLog("[TEST] QApplication created");
    
    MinimalMainWindow window;
    appendLifecycleLog("[TEST] MainWindow created");
    
    window.show();
    appendLifecycleLog("[TEST] MainWindow shown");
    
    appendLifecycleLog("[TEST] Entering event loop");
    int rc = app.exec();
    appendLifecycleLog("[TEST] Event loop exited");
    
    return rc;
}
