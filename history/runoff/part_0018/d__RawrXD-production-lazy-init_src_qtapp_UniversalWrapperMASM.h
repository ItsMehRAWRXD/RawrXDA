#pragma once
#include <QString>
#include <string>

class UniversalWrapperMASM {
public:
    enum class WrapperMode {
        PURE_MASM,
        HYBRID,
        STANDARD
    };

    enum class Format {
        UNKNOWN,
        GGUF,
        GGML,
        PYTORCH,
        SAFETENSORS,
        ONNX
    };

    explicit UniversalWrapperMASM(WrapperMode mode = WrapperMode::PURE_MASM) : m_mode(mode) {}
    ~UniversalWrapperMASM() = default;

    Format detectFormat(const QString& modelPath) {
        if (modelPath.endsWith(".gguf", Qt::CaseInsensitive)) return Format::GGUF;
        if (modelPath.endsWith(".bin", Qt::CaseInsensitive)) return Format::GGML;
        if (modelPath.endsWith(".pt", Qt::CaseInsensitive) || modelPath.endsWith(".pth", Qt::CaseInsensitive)) return Format::PYTORCH;
        if (modelPath.endsWith(".safetensors", Qt::CaseInsensitive)) return Format::SAFETENSORS;
        if (modelPath.endsWith(".onnx", Qt::CaseInsensitive)) return Format::ONNX;
        return Format::UNKNOWN;
    }

    bool loadModelAuto(const QString& modelPath) {
        m_lastError = "Not implemented";
        m_tempOutputPath = modelPath; // Mock implementation
        return true;
    }

    QString getLastError() const { return m_lastError; }
    QString getTempOutputPath() const { return m_tempOutputPath; }

private:
    WrapperMode m_mode;
    QString m_lastError;
    QString m_tempOutputPath;
};
