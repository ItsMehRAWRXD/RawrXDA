/**
 * @file RawrXD_Scintilla_Loader.cpp
 * @brief Dynamic loader and initializer for Scintilla in the Sovereign IDE.
 * 
 * Target: v15.3.0-SCINTILLA
 */

#include <windows.h>
#include <iostream>
#include "Scintilla.h"
#include "SciLexer.h"

typedef sptr_t (*SciFnDirect)(sptr_t ptr, unsigned int iMessage, uptr_t wParam, sptr_t lParam);

class ScintillaManager {
public:
    static bool LoadScintilla() {
        HMODULE hSci = LoadLibraryA("SciLexer.dll");
        if (!hSci) {
            std::cerr << "[SCINTILLA] Failed to load SciLexer.dll. Falling back to Edit." << std::endl;
            return false;
        }
        std::cout << "[SCINTILLA] SciLexer.dll loaded successfully." << std::endl;
        return true;
    }

    static void ConfigureMASMLexer(HWND hSci) {
        // Direct function access for maximum performance
        SciFnDirect fn = (SciFnDirect)SendMessage(hSci, SCI_GETDIRECTFUNCTION, 0, 0);
        sptr_t ptr = SendMessage(hSci, SCI_GETDIRECTPOINTER, 0, 0);

        auto sci = [&](unsigned int msg, uptr_t w = 0, sptr_t l = 0) {
            return fn(ptr, msg, w, l);
        };

        // Set Lexer to Assembly
        sci(SCI_SETLEXER, SCLEX_ASM);
        
        // MASM Keywords
        const char* keywords = "mov add sub push pop call ret lea xor jmp je jne jz jnz .code .data .data? .stack proc endp segment ends public extern extrn include includelib";
        sci(SCI_SETKEYWORDS, 0, (sptr_t)keywords);

        // Styling: Dark Mode
        sci(SCI_STYLESETBACK, STYLE_DEFAULT, 0x1E1E1E); // Background
        sci(SCI_STYLESETFORE, STYLE_DEFAULT, 0xD4D4D4); // Foreground
        sci(SCI_STYLESETFONT, STYLE_DEFAULT, (sptr_t)"Consolas");
        sci(SCI_STYLESETSIZE, STYLE_DEFAULT, 10);
        sci(SCI_STYLECLEARALL);

        // ASM Specific Highlights
        sci(SCI_STYLESETFORE, SCE_ASM_INSTRUCTION, 0x569CD6); // Blue
        sci(SCI_STYLESETFORE, SCE_ASM_REGISTER, 0x9CDCFE);    // Light Blue
        sci(SCI_STYLESETFORE, SCE_ASM_DIRECTIVE, 0xC586C0);   // Purple
        sci(SCI_STYLESETFORE, SCE_ASM_COMMENT, 0x6A9955);     // Green
        
        // Line Numbers
        sci(SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER);
        sci(SCI_SETMARGINWIDTHN, 0, 40);
    }
};

extern "C" __declspec(dllexport) HWND Create_Scintilla_Editor(HWND hParent, HINSTANCE hInstance) {
    if (!ScintillaManager::LoadScintilla()) return NULL;

    HWND hSci = CreateWindowExA(0, "Scintilla", "", 
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPCHILDREN,
        0, 0, 0, 0, hParent, (HMENU)201, hInstance, NULL);

    if (hSci) {
        ScintillaManager::ConfigureMASMLexer(hSci);
    }
    return hSci;
}
