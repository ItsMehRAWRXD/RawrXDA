// Integration test for UI components
#include "Win32_MainWindow.hpp"
#include "Win32_HexConsole.hpp"
#include "Win32_HotpatchManager.hpp"

int main() {
    RawrXD::Win32::MainWindow window(GetModuleHandle(nullptr));
    window.Create(L"Test Window");
    
    RawrXD::Win32::HexConsole console(window.GetHandle());
    RawrXD::Win32::HotpatchManager manager;
    
    console.AppendLog(L"Test message");
    
    Vector<uint8_t> testData = { 0x48, 0x89, 0x5C, 0x24 };
    console.DisplayHex(testData);
    
    return 0;
}
