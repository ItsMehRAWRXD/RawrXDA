#include "ModelConversionDialog.h"

ModelConversionDialog::ModelConversionDialog(const std::vector<std::string>& unsupportedTypes,
                                           const std::string& recommendedType,
                                           const std::string& modelPath,
                                           HWND parent)
    : m_unsupportedTypes(unsupportedTypes)
    , m_recommendedType(recommendedType)
    , m_modelPath(modelPath)
    , m_parent(parent)
{
}

ModelConversionDialog::Result ModelConversionDialog::exec()
{
    std::string msg = "The model uses unsupported quantization types: ";
    for(const auto& t : m_unsupportedTypes) msg += t + " ";
    msg += "\n\nRecommended: " + m_recommendedType;
    msg += "\n\nConversion is currently manual. Please use the CLI converter.";
    
    // In a full implementation, this would show a dialog with buttons.
    // For now, we just inform the user.
    MessageBoxA(m_parent, msg.c_str(), "Model Conversion Required", MB_OK | MB_ICONWARNING);
    
    return Cancelled;
}



