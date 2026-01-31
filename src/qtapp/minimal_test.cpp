

int main(int argc, char* argv[])
{
    
    QApplication app(argc, argv);
    
    void window;
    window.setWindowTitle("Minimal Qt Test");
    window.resize(400, 300);
    
    void* central = new void(&window);
    QVBoxLayout* layout = new QVBoxLayout(central);
    
    QLabel* label = new QLabel("Qt is working!", central);
    layout->addWidget(label);
    
    window.setCentralWidget(central);
    window.show();
    
    
    return app.exec();
}

