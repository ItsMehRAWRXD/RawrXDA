#include <iostream>
#include <QCoreApplication>
#include <QTimer>
#include <QThread>
#include <QDebug>

// Include the implementation we created - include header first
#include "src/qtapp/ai_workers.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    qDebug() << "Testing AI Workers Implementation...";
    
    // Test basic instantiation
    {
        // Mock classes for testing
        class MockAIDigestionEngine {
        public:
            bool processFile(const QString& filePath) {
                qDebug() << "Processing file:" << filePath;
                return true;
            }
        };

        MockAIDigestionEngine engine;
        AIDigestionWorker worker(&engine);
        qDebug() << "✓ AIDigestionWorker created successfully";
    }
    
    {
        class MockAITrainingPipeline {
        public:
            void prepareDataset() {}
            void trainEpoch() {}
        };

        MockAITrainingPipeline pipeline;
        AITrainingWorker worker(&pipeline);
        qDebug() << "✓ AITrainingWorker created successfully";
    }
    
    {
        AIWorkerManager manager;
        qDebug() << "✓ AIWorkerManager created successfully";
    }
    
    qDebug() << "All tests passed!";
    
    // Exit after a short delay
    QTimer::singleShot(100, [&app]() {
        app.quit();
    });
    
    return app.exec();
}
