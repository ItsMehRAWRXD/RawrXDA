// ollama_service_link_stub.cpp
// Link stubs for OllamaServiceManager (pre-existing source has compile errors).
// Satisfies linker requirements for Win32IDE_Core.cpp and Win32IDE_Commands.cpp.

#include "win32app/OllamaServiceManager.h"

// Forward‐declared in Win32IDE.h
class Win32IDE;

OllamaServiceManager::OllamaServiceManager(Win32IDE*) {}
OllamaServiceManager::~OllamaServiceManager() {}

// Win32IDE::createOllamaServicePane() declared in Win32IDE.h
#include "win32app/Win32IDE.h"
void Win32IDE::createOllamaServicePane() {
    // Ollama pane not yet wired — no‐op until OllamaServiceManager.cpp compiles.
}
