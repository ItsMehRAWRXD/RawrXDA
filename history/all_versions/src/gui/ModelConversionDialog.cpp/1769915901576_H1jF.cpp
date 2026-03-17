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
    // Explicit Logic: Check for converter tool presence
    std::string converterPath = "d:\\rawrxd\\bin\\converter.exe"; // hypothetical path
    bool toolsAvailable = false;
    FILE* f = fopen(converterPath.c_str(), "rb");
    if(f) { fclose(f); toolsAvailable = true; }
    
    if (toolsAvailable) {
        int response = MessageBoxA(m_parent, (msg + "\n\nDo you want to run the converter tool?").c_str(), "Model Conversion Required", MB_YESNO | MB_ICONQUESTION);
        if (response == IDYES) {
             // Basic spawn
             std::string cmd = converterPath + " \"" + m_modelPath + "\"";
             // system(cmd.c_str()); // blocking
             // Non-blocking would be better usually
             WinExec(cmd.c_str(), SW_SHOW); 
             return Convert;
        }
    } else {
        MessageBoxA(m_parent, msg.c_str(), "Model Conversion Required", MB_OK | MB_ICONWARNING);
    }
    
    return Cancelled;
}



