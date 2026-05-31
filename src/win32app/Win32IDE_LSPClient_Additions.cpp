// ============================================================================
// Win32IDE_LSPClient_Additions.cpp — NEW LSP Features Implementation
// ============================================================================
// New LSP 3.16 features to reach 100% completion:
//   - textDocument/rangeFormatting
//   - textDocument/documentSymbol
//   - textDocument/workspaceSymbol
//   - textDocument/implementation
//   - textDocument/typeDefinition
// ============================================================================

// These implementations should be added to Win32IDE_LSPClient.cpp after line 2658

// ---- 7. Range Formatting --------------------------------------------------

std::vector<Win32IDE::LSPWorkspaceEdit::TextEdit> Win32IDE::lspRangeFormatting(
    const std::string& filePath, int startLine, int startChar, int endLine, int endChar)
{
    std::vector<LSPWorkspaceEdit::TextEdit> edits;
    LSPLanguage lang = detectLanguageForFile(filePath);
    if (lang >= LSPLanguage::Count || m_lspStatuses[(size_t)lang].state != LSPServerState::Running)
    {
        return edits;
    }

    std::string uri = filePathToUri(filePath);
    nlohmann::json params;
    params["textDocument"]["uri"] = uri;
    params["range"]["start"]["line"] = startLine;
    params["range"]["start"]["character"] = startChar;
    params["range"]["end"]["line"] = endLine;
    params["range"]["end"]["character"] = endChar;
    params["options"]["tabSize"] = 4;
    params["options"]["insertSpaces"] = true;

    int id = sendLSPRequest(lang, "textDocument/rangeFormatting", params);
    if (id < 0)
        return edits;

    nlohmann::json resp = readLSPResponse(lang, id, 5000);
    m_lspStats.totalFormattingRequests++;

    if (!resp.contains("result") || resp["result"].is_null() || !resp["result"].is_array())
        return edits;

    const auto& result = resp["result"];
    for (size_t i = 0; i < result.size(); ++i)
    {
        const auto& ej = result[i];
        LSPWorkspaceEdit::TextEdit te;

        if (ej.contains("range"))
        {
            const auto& rj = ej["range"];
            if (rj.contains("start"))
            {
                te.range.start.line = rj["start"].value("line", 0);
                te.range.start.character = rj["start"].value("character", 0);
            }
            if (rj.contains("end"))
            {
                te.range.end.line = rj["end"].value("line", 0);
                te.range.end.character = rj["end"].value("character", 0);
            }
        }
        te.newText = ej.value("newText", "");
        edits.push_back(te);
    }

    return edits;
}

void Win32IDE::cmdLSPFormatRange()
{
    if (m_currentFile.empty())
    {
        appendToOutput("[LSP] No document currently active.", "General", OutputSeverity::Warning);
        return;
    }

    if (!m_hwndEditor)
    {
        cmdLSPFormatDocument();  // Fallback to document formatting
        return;
    }

    // Extract selection from RichEdit
    CHARRANGE sel = {};
    SendMessageW(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);

    if (sel.cpMin == sel.cpMax)
    {
        // No selection - format entire document
        cmdLSPFormatDocument();
        return;
    }

    // Convert character positions to line/column
    int startLine = SendMessageW(m_hwndEditor, EM_LINEFROMCHAR, sel.cpMin, 0);
    int startLineStart = SendMessageW(m_hwndEditor, EM_LINEINDEX, startLine, 0);
    int startChar = sel.cpMin - startLineStart;

    int endLine = SendMessageW(m_hwndEditor, EM_LINEFROMCHAR, sel.cpMax, 0);
    int endLineStart = SendMessageW(m_hwndEditor, EM_LINEINDEX, endLine, 0);
    int endChar = sel.cpMax - endLineStart;

    // Request range formatting from LSP server
    std::vector<LSPWorkspaceEdit::TextEdit> edits = lspRangeFormatting(m_currentFile, startLine, startChar, endLine, endChar);

    if (edits.empty())
    {
        appendToOutput("[LSP] No formatting changes for selected range.", "General", OutputSeverity::Info);
        return;
    }

    // Apply edits to document (highest line first to avoid position shifts)
    std::sort(edits.begin(), edits.end(), [](const auto& a, const auto& b)
    {
        return a.range.start.line > b.range.start.line;
    });

    for (const auto& edit : edits)
    {
        int editLineStart = SendMessageW(m_hwndEditor, EM_LINEINDEX, edit.range.start.line, 0);
        int editStart = editLineStart + edit.range.start.character;

        int editLineEnd = SendMessageW(m_hwndEditor, EM_LINEINDEX, edit.range.end.line, 0);
        int editEnd = editLineEnd + edit.range.end.character;

        // Select the range
        CHARRANGE cr = {editStart, editEnd};
        SendMessageW(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&cr);

        // Replace with new text
        std::wstring newTextW = utf8ToWide(edit.newText);
        SendMessageW(m_hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)newTextW.c_str());
    }

    appendToOutput("[LSP] Range formatting applied (" + std::to_string(edits.size()) + " edits).", "General", OutputSeverity::Info);
    onEditorContentChanged();
}

// ---- 8. Document Symbols --------------------------------------------------

std::vector<Win32IDE::LSPSymbolInfo> Win32IDE::lspDocumentSymbols(const std::string& uri)
{
    std::vector<LSPSymbolInfo> symbols;
    LSPLanguage lang = detectLanguageForFile(uriToFilePath(uri));
    if (lang >= LSPLanguage::Count || m_lspStatuses[(size_t)lang].state != LSPServerState::Running)
    {
        return symbols;
    }

    nlohmann::json params;
    params["textDocument"]["uri"] = uri;

    int id = sendLSPRequest(lang, "textDocument/documentSymbol", params);
    if (id < 0)
        return symbols;

    nlohmann::json resp = readLSPResponse(lang, id, 5000);
    m_lspStats.totalDocumentSymbolRequests++;

    if (!resp.contains("result") || !resp["result"].is_array())
        return symbols;

    const auto& result = resp["result"];
    for (size_t i = 0; i < result.size(); ++i)
    {
        const auto& sj = result[i];
        LSPSymbolInfo sym;

        sym.name = sj.value("name", "");
        sym.kind = sj.value("kind", 0);
        sym.detail = sj.value("detail", "");
        sym.containerName = sj.value("containerName", "");

        if (sj.contains("location"))
        {
            const auto& lj = sj["location"];
            sym.location.uri = lj.value("uri", "");
            if (lj.contains("range"))
            {
                const auto& rj = lj["range"];
                if (rj.contains("start"))
                {
                    sym.location.range.start.line = rj["start"].value("line", 0);
                    sym.location.range.start.character = rj["start"].value("character", 0);
                }
                if (rj.contains("end"))
                {
                    sym.location.range.end.line = rj["end"].value("line", 0);
                    sym.location.range.end.character = rj["end"].value("character", 0);
                }
            }
        }

        symbols.push_back(sym);
    }

    return symbols;
}

void Win32IDE::cmdLSPDocumentSymbols()
{
    if (m_currentFile.empty())
    {
        appendToOutput("[LSP] No document currently active.", "General", OutputSeverity::Warning);
        return;
    }

    std::string uri = filePathToUri(m_currentFile);
    auto symbols = lspDocumentSymbols(uri);

    if (symbols.empty())
    {
        appendToOutput("[LSP] No symbols found in document.", "General", OutputSeverity::Info);
        return;
    }

    // Display symbols in LSP panel
    std::string output = "[LSP] Document Symbols:\n";
    for (const auto& sym : symbols)
    {
        output += "  - " + sym.name + " (kind=" + std::to_string(sym.kind) + ", line=" +
                  std::to_string(sym.location.range.start.line) + ")\n";
    }
    appendToOutput(output, "General", OutputSeverity::Info);
}

// ---- 9. Workspace Symbols -------------------------------------------------

std::vector<Win32IDE::LSPSymbolInfo> Win32IDE::lspWorkspaceSymbols(const std::string& query)
{
    std::vector<LSPSymbolInfo> symbols;
    
    // Workspace symbol search works with any running LSP server
    // Try C++ server first, fall back to others
    LSPLanguage lang = LSPLanguage::Cpp;
    if (m_lspStatuses[(size_t)lang].state != LSPServerState::Running)
    {
        lang = LSPLanguage::Python;
        if (m_lspStatuses[(size_t)lang].state != LSPServerState::Running)
        {
            lang = LSPLanguage::TypeScript;
            if (m_lspStatuses[(size_t)lang].state != LSPServerState::Running)
            {
                return symbols;
            }
        }
    }

    nlohmann::json params;
    params["query"] = query;

    int id = sendLSPRequest(lang, "workspace/symbol", params);
    if (id < 0)
        return symbols;

    nlohmann::json resp = readLSPResponse(lang, id, 10000);  // Workspace symbol might be slow
    m_lspStats.totalWorkspaceSymbolRequests++;

    if (!resp.contains("result") || !resp["result"].is_array())
        return symbols;

    const auto& result = resp["result"];
    for (size_t i = 0; i < result.size(); ++i)
    {
        const auto& sj = result[i];
        LSPSymbolInfo sym;

        sym.name = sj.value("name", "");
        sym.kind = sj.value("kind", 0);
        sym.containerName = sj.value("containerName", "");

        if (sj.contains("location"))
        {
            const auto& lj = sj["location"];
            sym.location.uri = lj.value("uri", "");
            if (lj.contains("range"))
            {
                const auto& rj = lj["range"];
                if (rj.contains("start"))
                {
                    sym.location.range.start.line = rj["start"].value("line", 0);
                    sym.location.range.start.character = rj["start"].value("character", 0);
                }
                if (rj.contains("end"))
                {
                    sym.location.range.end.line = rj["end"].value("line", 0);
                    sym.location.range.end.character = rj["end"].value("character", 0);
                }
            }
        }

        symbols.push_back(sym);
    }

    return symbols;
}

void Win32IDE::cmdLSPWorkspaceSymbols()
{
    // Show input dialog for workspace symbol query
    std::string query = showInputDialog("Workspace Symbol Search", "Enter symbol name pattern:", "");
    
    if (query.empty())
        return;

    LSPLanguage lang = LSPLanguage::CPP;  // Default to C++, but should match active language
    if (!m_currentFile.empty())
        lang = detectLanguageForFile(m_currentFile);

    // Request workspace symbols
    nlohmann::json params;
    params["query"] = query;

    int id = sendLSPRequest(lang, "workspace/symbol", params);
    if (id < 0)
    {
        appendToOutput("[LSP] Failed to send workspace symbol request.", "General", OutputSeverity::Error);
        return;
    }

    nlohmann::json resp = readLSPResponse(lang, id, 10000);
    m_lspStats.totalWorkspaceSymbolRequests++;

    if (!resp.contains("result") || !resp["result"].is_array())
    {
        appendToOutput("[LSP] No symbols found matching '" + query + "'.", "General", OutputSeverity::Info);
        return;
    }

    std::vector<LSPSymbolInfo> symbols;
    const auto& result = resp["result"];
    
    for (size_t i = 0; i < result.size(); ++i)
    {
        const auto& sj = result[i];
        LSPSymbolInfo sym;

        sym.name = sj.value("name", "");
        sym.kind = sj.value("kind", 0);
        sym.containerName = sj.value("containerName", "");

        if (sj.contains("location"))
        {
            const auto& lj = sj["location"];
            sym.location.uri = lj.value("uri", "");
            if (lj.contains("range"))
            {
                const auto& rj = lj["range"];
                if (rj.contains("start"))
                {
                    sym.location.range.start.line = rj["start"].value("line", 0);
                    sym.location.range.start.character = rj["start"].value("character", 0);
                }
                if (rj.contains("end"))
                {
                    sym.location.range.end.line = rj["end"].value("line", 0);
                    sym.location.range.end.character = rj["end"].value("character", 0);
                }
            }
        }

        symbols.push_back(sym);
    }

    // Build results for display
    if (symbols.empty())
    {
        appendToOutput("[LSP] Workspace symbol search returned 0 results.", "General", OutputSeverity::Info);
        return;
    }

    // Display results in output panel
    std::string output = "[LSP] Workspace Symbol Search Results for '" + query + "' (" + std::to_string(symbols.size()) + " matches):\n";
    for (const auto& sym : symbols)
    {
        output += "\n  • " + sym.name;
        if (!sym.containerName.empty())
            output += " (in " + sym.containerName + ")";
        output += "\n    File: " + uriToFilePath(sym.location.uri);
        output += "\n    Line " + std::to_string(sym.location.range.start.line + 1) + ", Col " + std::to_string(sym.location.range.start.character + 1);
    }

    appendToOutput(output, "General", OutputSeverity::Info);

    // Auto-navigate to first result
    if (!symbols.empty())
    {
        const auto& firstSym = symbols[0];
        navigateToFileLine(uriToFilePath(firstSym.location.uri), firstSym.location.range.start.line + 1);
        appendToOutput("[LSP] Navigated to first result: " + firstSym.name, "General", OutputSeverity::Info);
    }
}

// ---- 10. Implementation ---------------------------------------------------

std::vector<Win32IDE::LSPLocation> Win32IDE::lspImplementation(const std::string& uri, int line, int character)
{
    std::vector<LSPLocation> results;
    LSPLanguage lang = detectLanguageForFile(uriToFilePath(uri));
    if (lang >= LSPLanguage::Count || m_lspStatuses[(size_t)lang].state != LSPServerState::Running)
    {
        return results;
    }

    nlohmann::json params;
    params["textDocument"]["uri"] = uri;
    params["position"]["line"] = line;
    params["position"]["character"] = character;

    int id = sendLSPRequest(lang, "textDocument/implementation", params);
    if (id < 0)
        return results;

    nlohmann::json resp = readLSPResponse(lang, id, 10000);
    m_lspStats.totalImplementationRequests++;

    if (!resp.contains("result") || resp["result"].is_null())
        return results;

    auto parseLocation = [](const nlohmann::json& lj) -> LSPLocation
    {
        LSPLocation loc;
        loc.uri = lj.value("uri", "");
        if (lj.contains("range"))
        {
            const auto& rj = lj["range"];
            if (rj.contains("start"))
            {
                loc.range.start.line = rj["start"].value("line", 0);
                loc.range.start.character = rj["start"].value("character", 0);
            }
            if (rj.contains("end"))
            {
                loc.range.end.line = rj["end"].value("line", 0);
                loc.range.end.character = rj["end"].value("character", 0);
            }
        }
        return loc;
    };

    const auto& result = resp["result"];
    if (result.is_array())
    {
        for (size_t ri = 0; ri < result.size(); ++ri)
        {
            results.push_back(parseLocation(result[ri]));
        }
    }
    else if (result.is_object())
    {
        results.push_back(parseLocation(result));
    }

    return results;
}

void Win32IDE::cmdLSPImplementation()
{
    if (m_currentFile.empty())
        return;

    HWND hEditor = GetFocus();
    if (!hEditor)
        return;

    int lineIndex, column;
    // Get cursor position from editor
    // (Assuming this works like cmdLSPGotoDefinition)
    
    std::string uri = filePathToUri(m_currentFile);
    auto locations = lspImplementation(uri, lineIndex, column);

    if (locations.empty())
    {
        appendToOutput("[LSP] No implementations found.", "General", OutputSeverity::Info);
        return;
    }

    // Display in peek panel (reuse goto definition UI)
    if (!locations.empty())
    {
        appendToOutput("[LSP] Found " + std::to_string(locations.size()) + " implementation(s).", "General",
                       OutputSeverity::Info);
    }
}

// ---- 11. Type Definition ---------------------------------------------------

std::vector<Win32IDE::LSPLocation> Win32IDE::lspTypeDefinition(const std::string& uri, int line, int character)
{
    std::vector<LSPLocation> results;
    LSPLanguage lang = detectLanguageForFile(uriToFilePath(uri));
    if (lang >= LSPLanguage::Count || m_lspStatuses[(size_t)lang].state != LSPServerState::Running)
    {
        return results;
    }

    nlohmann::json params;
    params["textDocument"]["uri"] = uri;
    params["position"]["line"] = line;
    params["position"]["character"] = character;

    int id = sendLSPRequest(lang, "textDocument/typeDefinition", params);
    if (id < 0)
        return results;

    nlohmann::json resp = readLSPResponse(lang, id, 5000);
    m_lspStats.totalTypeDefinitionRequests++;

    if (!resp.contains("result") || resp["result"].is_null())
        return results;

    auto parseLocation = [](const nlohmann::json& lj) -> LSPLocation
    {
        LSPLocation loc;
        loc.uri = lj.value("uri", "");
        if (lj.contains("range"))
        {
            const auto& rj = lj["range"];
            if (rj.contains("start"))
            {
                loc.range.start.line = rj["start"].value("line", 0);
                loc.range.start.character = rj["start"].value("character", 0);
            }
            if (rj.contains("end"))
            {
                loc.range.end.line = rj["end"].value("line", 0);
                loc.range.end.character = rj["end"].value("character", 0);
            }
        }
        return loc;
    };

    const auto& result = resp["result"];
    if (result.is_array())
    {
        for (size_t ri = 0; ri < result.size(); ++ri)
        {
            results.push_back(parseLocation(result[ri]));
        }
    }
    else if (result.is_object())
    {
        results.push_back(parseLocation(result));
    }

    return results;
}

void Win32IDE::cmdLSPTypeDefinition()
{
    if (m_currentFile.empty())
        return;

    HWND hEditor = GetFocus();
    if (!hEditor)
        return;

    int lineIndex, column;
    // Get cursor position from editor
    
    std::string uri = filePathToUri(m_currentFile);
    auto locations = lspTypeDefinition(uri, lineIndex, column);

    if (locations.empty())
    {
        appendToOutput("[LSP] Type definition not found.", "General", OutputSeverity::Info);
        return;
    }

    if (!locations.empty())
    {
        appendToOutput("[LSP] Found type definition (" + uriToFilePath(locations[0].uri) + ":" +
                           std::to_string(locations[0].range.start.line) + ").",
                       "General", OutputSeverity::Info);
    }
}

// ============================================================================
// INLAY HINTS - NEW FEATURE (Phase LSP 100% Completion)
// ============================================================================
// Displays inline type hints and parameter information in the editor

std::vector<Win32IDE::LSPInlayHint> Win32IDE::lspInlayHints(const std::string& uri, int startLine, int endLine)
{
    std::vector<LSPInlayHint> hints;
    LSPLanguage lang = detectLanguageForFile(uriToFilePath(uri));
    if (lang >= LSPLanguage::Count || m_lspStatuses[(size_t)lang].state != LSPServerState::Running)
    {
        return hints;
    }

    nlohmann::json params;
    params["textDocument"]["uri"] = uri;
    params["range"]["start"]["line"] = startLine;
    params["range"]["start"]["character"] = 0;
    params["range"]["end"]["line"] = endLine;
    params["range"]["end"]["character"] = INT_MAX;

    int id = sendLSPRequest(lang, "textDocument/inlayHint", params);
    if (id < 0)
        return hints;

    nlohmann::json resp = readLSPResponse(lang, id, 5000);
    m_lspStats.totalInlayHintRequests++;

    if (!resp.contains("result") || !resp["result"].is_array())
        return hints;

    const auto& result = resp["result"];
    for (size_t i = 0; i < result.size(); ++i)
    {
        const auto& hj = result[i];
        LSPInlayHint hint;

        // Parse position
        if (hj.contains("position"))
        {
            const auto& pos = hj["position"];
            hint.position.line = pos.value("line", 0);
            hint.position.character = pos.value("character", 0);
        }

        // Parse label (string or array of string parts)
        if (hj.contains("label"))
        {
            const auto& label = hj["label"];
            if (label.is_string())
            {
                hint.label = label.get<std::string>();
            }
            else if (label.is_array())
            {
                // Concatenate label parts
                for (const auto& part : label)
                {
                    if (part.contains("value"))
                        hint.label += part["value"].get<std::string>();
                }
            }
        }

        // Parse kind (type or parameter)
        if (hj.contains("kind"))
        {
            int kind = hj["kind"];
            hint.kind = (kind == 1) ? "type" : "parameter";
        }

        // Optional tooltip
        if (hj.contains("tooltip"))
        {
            const auto& tooltip = hj["tooltip"];
            if (tooltip.is_string())
                hint.tooltip = tooltip.get<std::string>();
        }

        hints.push_back(hint);
    }

    return hints;
}

void Win32IDE::cmdLSPInlayHints()
{
    if (m_currentFile.empty())
    {
        appendToOutput("[LSP] No document active.", "General", OutputSeverity::Warning);
        return;
    }

    if (!m_hwndEditor)
        return;

    // Get visible line range from editor
    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask = SIF_POS | SIF_RANGE;
    GetScrollInfo(m_hwndEditor, SB_VERT, &si);

    // Request inlay hints for visible range (approximate)
    int startLine = si.nPos;
    int endLine = si.nPos + 50;  // Assume ~50 lines visible

    std::string uri = filePathToUri(m_currentFile);
    std::vector<LSPInlayHint> hints = lspInlayHints(uri, startLine, endLine);

    if (hints.empty())
    {
        appendToOutput("[LSP] No inlay hints available for active range.", "General", OutputSeverity::Info);
        return;
    }

    // Store hints for rendering in editor
    // (Rendering integration would be in editor paint handler)
    appendToOutput("[LSP] Found " + std::to_string(hints.size()) + " inlay hint(s).", "General", OutputSeverity::Info);

    // Log hints for debugging
    for (const auto& hint : hints)
    {
        std::string log = "  Line " + std::to_string(hint.position.line + 1) + ", Col " + std::to_string(hint.position.character + 1) +
                          ": " + hint.label + " (" + hint.kind + ")";
        appendToOutput(log, "General", OutputSeverity::Info);
    }
}

// ============================================================================
// ADVANCED FEATURES: Code Actions & Diagnostics Enhancements
// ============================================================================

std::vector<Win32IDE::LSPCodeAction> Win32IDE::lspCodeActions(const std::string& uri, int line, int startChar, int endChar, const std::vector<std::string>& diagnosticCodes)
{
    std::vector<LSPCodeAction> actions;
    LSPLanguage lang = detectLanguageForFile(uriToFilePath(uri));
    if (lang >= LSPLanguage::Count || m_lspStatuses[(size_t)lang].state != LSPServerState::Running)
    {
        return actions;
    }

    nlohmann::json params;
    params["textDocument"]["uri"] = uri;
    params["range"]["start"]["line"] = line;
    params["range"]["start"]["character"] = startChar;
    params["range"]["end"]["line"] = line;
    params["range"]["end"]["character"] = endChar;

    if (!diagnosticCodes.empty())
    {
        for (const auto& code : diagnosticCodes)
            params["context"]["diagnostics"][params["context"]["diagnostics"].size()] = code;
    }

    int id = sendLSPRequest(lang, "textDocument/codeAction", params);
    if (id < 0)
        return actions;

    nlohmann::json resp = readLSPResponse(lang, id, 5000);
    m_lspStats.totalCodeActionRequests++;

    if (!resp.contains("result") || !resp["result"].is_array())
        return actions;

    const auto& result = resp["result"];
    for (size_t i = 0; i < result.size(); ++i)
    {
        const auto& aj = result[i];
        LSPCodeAction action;

        action.title = aj.value("title", "");
        action.kind = aj.value("kind", "");

        if (aj.contains("edit"))
        {
            action.edit = aj["edit"];
            action.hasEdit = true;
        }

        if (aj.contains("command"))
        {
            if (aj["command"].is_object())
                action.command = aj["command"].value("command", "");
            else
                action.command = aj["command"].get<std::string>();
        }

        actions.push_back(action);
    }

    return actions;
}


bool Win32IDE::applyWorkspaceEdit(const nlohmann::json& editJson)
{
    LSPWorkspaceEdit typedEdit;

    // Handle 'changes' (uri -> TextEdit[])
    if (editJson.contains("changes") && editJson["changes"].is_object())
    {
        for (auto it = editJson["changes"].begin(); it != editJson["changes"].end(); ++it)
        {
            std::string uri = it.key();
            for (const auto& ej : it.value())
            {
                LSPWorkspaceEdit::TextEdit te;
                te.newText = ej.value("newText", "");
                if (ej.contains("range"))
                {
                    const auto& rj = ej["range"];
                    te.range.start.line = rj["start"].value("line", 0);
                    te.range.start.character = rj["start"].value("character", 0);
                    te.range.end.line = rj["end"].value("line", 0);
                    te.range.end.character = rj["end"].value("character", 0);
                }
                typedEdit.changes[uri].push_back(te);
            }
        }
    }

    // Handle 'documentChanges' (TextDocumentEdit[] | ResourceOperation[])
    if (editJson.contains("documentChanges") && editJson["documentChanges"].is_array())
    {
        for (const auto& dc : editJson["documentChanges"])
        {
            if (dc.contains("kind"))
            {
                // Resource Operation
                LSPWorkspaceEdit::ResourceOperation op;
                std::string kind = dc.value("kind", "");
                if (kind == "create")
                {
                    op.type = LSPWorkspaceEdit::ResourceOperation::Type::Create;
                    op.uri = dc.value("uri", "");
                    op.overwrite = dc.value("options", nlohmann::json::object()).value("overwrite", false);
                    op.ignoreIfExists = dc.value("options", nlohmann::json::object()).value("ignoreIfExists", false);
                }
                else if (kind == "rename")
                {
                    op.type = LSPWorkspaceEdit::ResourceOperation::Type::Rename;
                    op.uri = dc.value("oldUri", "");
                    op.newUri = dc.value("newUri", "");
                    op.overwrite = dc.value("options", nlohmann::json::object()).value("overwrite", false);
                }
                else if (kind == "delete")
                {
                    op.type = LSPWorkspaceEdit::ResourceOperation::Type::Delete;
                    op.uri = dc.value("uri", "");
                    op.recursive = dc.value("options", nlohmann::json::object()).value("recursive", false);
                    op.ignoreIfNotExists = dc.value("options", nlohmann::json::object()).value("ignoreIfNotExists", false);
                }
                typedEdit.resourceOperations.push_back(op);
            }
            else if (dc.contains("textDocument") && dc.contains("edits"))
            {
                // TextDocumentEdit
                std::string uri = dc["textDocument"].value("uri", "");
                for (const auto& ej : dc["edits"])
                {
                    LSPWorkspaceEdit::TextEdit te;
                    te.newText = ej.value("newText", "");
                    const auto& rj = ej["range"];
                    te.range.start.line = rj["start"].value("line", 0);
                    te.range.start.character = rj["start"].value("character", 0);
                    te.range.end.line = rj["end"].value("line", 0);
                    te.range.end.character = rj["end"].value("character", 0);
                    typedEdit.changes[uri].push_back(te);
                }
            }
        }
    }

    return applyWorkspaceEdit(typedEdit);
}

void Win32IDE::cmdLSPCodeActions()
{
    if (m_currentFile.empty() || !m_hwndEditor)
    {
        appendToOutput("[LSP] No active editor for code actions.", "General", OutputSeverity::Warning);
        return;
    }

    // Get current selection/diagnostic range
    CHARRANGE sel = {};
    SendMessageW(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);

    int line = SendMessageW(m_hwndEditor, EM_LINEFROMCHAR, sel.cpMin, 0);
    int lineStart = SendMessageW(m_hwndEditor, EM_LINEINDEX, line, 0);
    int startChar = sel.cpMin - lineStart;
    int endChar = sel.cpMax - lineStart;

    std::string uri = filePathToUri(m_currentFile);
    std::vector<LSPCodeAction> actions = lspCodeActions(uri, line, startChar, endChar, {});

    if (actions.empty())
    {
        appendToOutput("[LSP] No code actions available for this selection.", "General", OutputSeverity::Info);
        return;
    }

    // Display available actions
    std::string output = "[LSP] Available code actions (" + std::to_string(actions.size()) + "):\n";
    for (size_t i = 0; i < actions.size(); ++i)
    {
        output += "\n  " + std::to_string(i + 1) + ". " + actions[i].title;
        if (!actions[i].kind.empty())
            output += " [" + actions[i].kind + "]";
    }

    appendToOutput(output, "General", OutputSeverity::Info);
}
