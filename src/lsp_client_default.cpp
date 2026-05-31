// lsp_client_default.cpp - Default LSPClient implementations when lsp_client_incremental is not linked

#include "lsp_client.h"

namespace RawrXD {

void LSPClient::sendIncrementalUpdate(const std::string& uri, int64_t version, 
                                      const std::string& oldText, const std::string& newText) {
    // Default implementation: send full document sync notification
    if (!m_transport || !m_initialized) return;
    
    nlohmann::json params;
    params["textDocument"]["uri"] = uri;
    params["textDocument"]["version"] = version;
    
    // For incremental sync, compute diff (simplified: send full content)
    params["contentChanges"] = nlohmann::json::array();
    nlohmann::json change;
    change["text"] = newText;
    params["contentChanges"].push_back(change);
    
    sendNotification("textDocument/didChange", params);
}
void LSPClient::cancelRequest(const std::string& id) { m_pendingCancellations[id] = true; }

} // namespace RawrXD
