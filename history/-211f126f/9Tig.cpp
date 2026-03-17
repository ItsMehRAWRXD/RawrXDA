#include <QApplication>
#include <QMainWindow>
#include <QPushButton>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QLabel>
#include <QMessageBox>
#include <QWidget>
#include "masm_agentic_bridge.hpp"

class AgenticIDE : public QMainWindow {
    Q_OBJECT
    
public:
    AgenticIDE(QWidget *parent = nullptr) : QMainWindow(parent) {
        setWindowTitle("RawrXD Agentic IDE - Production");
        resize(1200, 800);
        
        // Central widget
        QWidget *central = new QWidget(this);
        QVBoxLayout *layout = new QVBoxLayout(central);
        
        // Status label
        statusLabel = new QLabel("Initializing MASM Agentic Core...", this);
        layout->addWidget(statusLabel);
        
        // Output text
        outputText = new QTextEdit(this);
        outputText->setReadOnly(true);
        layout->addWidget(outputText);
        
        // Control buttons
        QHBoxLayout *btnLayout = new QHBoxLayout();
        
        initBtn = new QPushButton("Initialize Agentic Core", this);
        connect(initBtn, &QPushButton::clicked, this, &AgenticIDE::initializeCore);
        btnLayout->addWidget(initBtn);
        
        loadModelBtn = new QPushButton("Load GGUF Model", this);
        loadModelBtn->setEnabled(false);
        connect(loadModelBtn, &QPushButton::clicked, this, &AgenticIDE::loadModel);
        btnLayout->addWidget(loadModelBtn);
        
        execToolBtn = new QPushButton("Execute Tool", this);
        execToolBtn->setEnabled(false);
        connect(execToolBtn, &QPushButton::clicked, this, &AgenticIDE::executeTool);
        btnLayout->addWidget(execToolBtn);
        
        layout->addLayout(btnLayout);
        setCentralWidget(central);
        
        // Initialize bridge
        bridge = new MASMAgenticBridge(this);
        
        // Connect signals
        connect(bridge, &MASMAgenticBridge::modelLoaded, this, [this](const QString& model) {
            outputText->append(QString("✓ Model loaded: %1").arg(model));
            execToolBtn->setEnabled(true);
        });
        
        connect(bridge, &MASMAgenticBridge::modelSwapped, this, [this](int modelID) {
            outputText->append(QString("✓ Hot-swapped to model ID: %1").arg(modelID));
        });
        
        connect(bridge, &MASMAgenticBridge::toolExecuted, this, [this](int toolId, bool success) {
            outputText->append(QString("✓ Tool %1: %2").arg(toolId).arg(success ? "Success" : "Failed"));
        });
        
        connect(bridge, &MASMAgenticBridge::errorOccurred, this, [this](const QString& error) {
            outputText->append(QString("❌ Error: %1").arg(error));
        });
        
        statusLabel->setText("Ready - Click 'Initialize Agentic Core' to begin");
    }
    
private slots:
    void initializeCore() {
        outputText->append("Initializing MASM agentic core...");
        
        if (bridge->initializeIDE()) {
            outputText->append("✓ IDE Master initialized");
            statusLabel->setText("Status: Agentic core active with 58 tools");
            loadModelBtn->setEnabled(true);
            initBtn->setEnabled(false);
        } else {
            outputText->append("❌ Failed to initialize IDE Master");
            QMessageBox::critical(this, "Initialization Error", 
                "Failed to initialize MASM agentic core. Check DLL exports.");
        }
    }
    
    void loadModel() {
        // Example: Load a test model
        QString modelPath = "C:/models/test.gguf";
        outputText->append(QString("Loading model: %1").arg(modelPath));
        
        if (bridge->loadModel(modelPath)) {
            outputText->append("✓ Model loading initiated");
        } else {
            outputText->append("❌ Failed to load model");
        }
    }
    
    void executeTool() {
        // Example: Execute FileRead tool (ID 0)
        QVariantMap params;
        params["path"] = "C:/test.txt";
        
        outputText->append("Executing FileRead tool (ID 0)...");
        
        bool success = bridge->executeTool(0, params);  // FileRead is tool ID 0
        if (success) {
            outputText->append("✓ Tool executed successfully");
        } else {
            outputText->append("❌ Tool execution failed");
        }
    }
    
private:
    MASMAgenticBridge *bridge;
    QLabel *statusLabel;
    QTextEdit *outputText;
    QPushButton *initBtn;
    QPushButton *loadModelBtn;
    QPushButton *execToolBtn;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    AgenticIDE ide;
    ide.show();
    
    return app.exec();
}

#include "agentic_ide_main.moc"
