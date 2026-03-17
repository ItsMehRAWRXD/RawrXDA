#include "test_model_generator.hpp"
#include <QDebug>
#include <QFile>
#include <QDir>

TestModelGenerator::TestModelGenerator(QObject* parent)
    : QObject(parent)
{
    // Set paths relative to current directory
    m_safeTensorsPath = QDir::currentPath() + "/test_model.safetensors";
    m_pytorchPath = QDir::currentPath() + "/test_model.pt";
}

bool TestModelGenerator::generateSafeTensorsModel()
{
    qDebug() << "Generating test SafeTensors model...";
    
    // Call MASM function
    int result = GenerateTestSafeTensors();
    
    if (result == 1) {
        qDebug() << "✅ SafeTensors model generated successfully:" << m_safeTensorsPath;
        emit modelGenerated("safetensors", true);
        return true;
    } else {
        qDebug() << "❌ Failed to generate SafeTensors model";
        emit modelGenerated("safetensors", false);
        return false;
    }
}

bool TestModelGenerator::generatePyTorchModel()
{
    qDebug() << "Generating test PyTorch model...";
    
    // Call MASM function
    int result = GenerateTestPyTorch();
    
    if (result == 1) {
        qDebug() << "✅ PyTorch model generated successfully:" << m_pytorchPath;
        emit modelGenerated("pytorch", true);
        return true;
    } else {
        qDebug() << "❌ Failed to generate PyTorch model";
        emit modelGenerated("pytorch", false);
        return false;
    }
}

bool TestModelGenerator::generateAllTestModels()
{
    qDebug() << "Generating all test models...";
    
    bool safeTensorsSuccess = generateSafeTensorsModel();
    bool pytorchSuccess = generatePyTorchModel();
    
    if (safeTensorsSuccess && pytorchSuccess) {
        qDebug() << "✅ All test models generated successfully";
        return true;
    } else {
        qDebug() << "❌ Some test models failed to generate";
        return false;
    }
}