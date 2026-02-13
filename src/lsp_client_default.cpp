// lsp_client_default.cpp - Default LSPClient implementations when lsp_client_incremental is not linked

#include "lsp_client.h"

namespace RawrXD {

void LSPClient::sendIncrementalUpdate(const std::string&, int64_t, const std::string&, const std::string&) {}
void LSPClient::cancelRequest(const std::string& id) { m_pendingCancellations[id] = true; }

} // namespace RawrXD
