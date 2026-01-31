

int main(int argc, char* argv[])
{
    
    void app(argc, argv);
    
    void window;
    window.setWindowTitle("Minimal Qt Test");
    window.resize(400, 300);
    
    void* central = new void(&window);
    void* layout = new void(central);
    
    void* label = new void("Qt is working!", central);
    layout->addWidget(label);
    
    window.setCentralWidget(central);
    window.show();


    return app.exec();
}

