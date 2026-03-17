// Compatibility wrapper to ensure AUTOGEN/MOC find the canonical LSP header.
// Historically some translation units included "src/lsp_client.h" directly.
// Forward to the public header in include/ which already includes <QObject>.
#pragma once

#include "../include/lsp_client.h"
