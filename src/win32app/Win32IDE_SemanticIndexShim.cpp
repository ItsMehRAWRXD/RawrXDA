#include "Win32IDE.h"

void Win32IDE::showSemanticIndex() {
    appendToOutput("[SemanticIndex] UI view is initializing. Use build/search commands to populate index data.\n",
                   "Semantic", OutputSeverity::Info);
}
