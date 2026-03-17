#pragma once

#include <string>
#include <vector>
#include <windows.h>
#include <cstdint>

class ModelConversionDialog {
public:
    enum Result {
        Converted,
        Cancelled,
        Failed
    };

    ModelConversionDialog(const std::vector<std::string>& unsupportedTypes,
                          const std::string& recommendedType,
                          const std::string& modelPath,
                          HWND parent);
    
    Result exec();
    std::string convertedModelPath() const { return m_convertedPath; }

private:
    std::vector<std::string> m_unsupportedTypes;
    std::string m_recommendedType;
    std::string m_modelPath;
    HWND m_parent;
    std::string m_convertedPath;
};


