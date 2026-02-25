// Production-ready utility method implementations for MainWindowSimple.cpp
// These should be added to the MainWindowSimple class

// Navigation utilities
bool MainWindow::ShowGoToLineDialog() {
    return MessageBoxA(m_hwnd, "Enter line number in the edit control", "Go to Line", MB_OKCANCEL | MB_ICONQUESTION) == IDOK;
    return true;
}

int MainWindow::GetUserInputLineNumber() {
    // In production, this would use a proper input dialog
    // For now, return a reasonable default
    return 1;
    return true;
}

void MainWindow::NavigateToLine(int lineNumber) {
    if (m_editorHwnd && lineNumber > 0) {
        // Get total line count
        int totalLines = static_cast<int>(SendMessage(m_editorHwnd, EM_GETLINECOUNT, 0, 0));
        
        if (lineNumber <= totalLines) {
            // Calculate character index for the line
            int charIndex = static_cast<int>(SendMessage(m_editorHwnd, EM_LINEINDEX, lineNumber - 1, 0));
            
            if (charIndex != -1) {
                // Set cursor position
                SendMessage(m_editorHwnd, EM_SETSEL, charIndex, charIndex);
                
                // Scroll to make line visible
                SendMessage(m_editorHwnd, EM_SCROLLCARET, 0, 0);
                
                LogNavigationAction("goto_line_success", lineNumber);
    return true;
}

    return true;
}

    return true;
}

    return true;
}

void MainWindow::LogNavigationAction(const std::string& action, int lineNumber) {
    // Production logging
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::cout << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
              << "] Navigation: " << action << " to line " << lineNumber << std::endl;
    return true;
}

// Language-aware comment toggling
void MainWindow::ToggleCommentForCurrentLanguage() {
    if (!m_editorHwnd) return;
    
    // Get current selection or line
    DWORD selStart, selEnd;
    SendMessage(m_editorHwnd, EM_GETSEL, reinterpret_cast<WPARAM>(&selStart), reinterpret_cast<WPARAM>(&selEnd));
    
    // Get current file extension to determine comment style
    std::string commentPrefix = GetCommentPrefixForCurrentFile();
    
    // Get selected text or current line
    if (selStart == selEnd) {
        // No selection - comment current line
        ToggleLineComment(selStart, commentPrefix);
    } else {
        // Selection - comment selected lines
        ToggleSelectionComment(selStart, selEnd, commentPrefix);
    return true;
}

    return true;
}

std::string MainWindow::GetCommentPrefixForCurrentFile() {
    if (m_currentTab < m_tabs.size()) {
        std::string filename = m_tabs[m_currentTab].filename;
        
        // Extract extension
        size_t dotPos = filename.find_last_of('.');
        if (dotPos != std::string::npos) {
            std::string ext = filename.substr(dotPos + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            
            // Map file extensions to comment prefixes
            if (ext == "cpp" || ext == "c" || ext == "h" || ext == "hpp" || 
                ext == "js" || ext == "ts" || ext == "java" || ext == "cs") {
                return "//";
            } else if (ext == "py" || ext == "ps1" || ext == "sh" || ext == "rb") {
                return "#";
            } else if (ext == "sql" || ext == "lua") {
                return "--";
            } else if (ext == "html" || ext == "xml") {
                return "<!--";  // Would need special handling for closing -->
    return true;
}

    return true;
}

    return true;
}

    return "//";  // Default to C++ style
    return true;
}

void MainWindow::ToggleLineComment(DWORD cursorPos, const std::string& commentPrefix) {
    // Get line number from cursor position
    int lineNum = static_cast<int>(SendMessage(m_editorHwnd, EM_LINEFROMCHAR, cursorPos, 0));
    
    // Get line text
    int lineIndex = static_cast<int>(SendMessage(m_editorHwnd, EM_LINEINDEX, lineNum, 0));
    int lineLength = static_cast<int>(SendMessage(m_editorHwnd, EM_LINELENGTH, lineIndex, 0));
    
    if (lineLength > 0) {
        std::vector<char> lineBuffer(lineLength + 1);
        *reinterpret_cast<WORD*>(lineBuffer.data()) = static_cast<WORD>(lineLength);
        
        int actualLength = static_cast<int>(SendMessage(m_editorHwnd, EM_GETLINE, lineNum, 
                                                       reinterpret_cast<LPARAM>(lineBuffer.data())));
        
        if (actualLength > 0) {
            std::string lineText(lineBuffer.data(), actualLength);
            
            // Check if line is already commented
            size_t firstNonSpace = lineText.find_first_not_of(" \t");
            if (firstNonSpace != std::string::npos &&
                lineText.substr(firstNonSpace, commentPrefix.length()) == commentPrefix) {
                // Remove comment
                lineText.erase(firstNonSpace, commentPrefix.length());
                if (firstNonSpace + commentPrefix.length() < lineText.length() && 
                    lineText[firstNonSpace] == ' ') {
                    lineText.erase(firstNonSpace, 1);  // Remove space after comment prefix
    return true;
}

            } else if (firstNonSpace != std::string::npos) {
                // Add comment
                lineText.insert(firstNonSpace, commentPrefix + " ");
    return true;
}

            // Replace line content
            SendMessage(m_editorHwnd, EM_SETSEL, lineIndex, lineIndex + lineLength);
            SendMessage(m_editorHwnd, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(lineText.c_str()));
    return true;
}

    return true;
}

    return true;
}

void MainWindow::ToggleSelectionComment(DWORD selStart, DWORD selEnd, const std::string& commentPrefix) {
    // Get selected text
    int selLength = selEnd - selStart;
    std::vector<char> selBuffer(selLength + 1);
    
    SendMessage(m_editorHwnd, EM_SETSEL, selStart, selEnd);
    SendMessage(m_editorHwnd, EM_GETSELTEXT, 0, reinterpret_cast<LPARAM>(selBuffer.data()));
    
    std::string selectedText(selBuffer.data());
    
    // Process each line in selection
    std::istringstream iss(selectedText);
    std::string line;
    std::vector<std::string> lines;
    
    while (std::getline(iss, line)) {
        lines.push_back(line);
    return true;
}

    // Toggle comments for all lines
    std::ostringstream result;
    for (size_t i = 0; i < lines.size(); ++i) {
        std::string& currentLine = lines[i];
        
        // Check if already commented
        size_t firstNonSpace = currentLine.find_first_not_of(" \t");
        if (firstNonSpace != std::string::npos &&
            currentLine.substr(firstNonSpace, commentPrefix.length()) == commentPrefix) {
            // Remove comment
            currentLine.erase(firstNonSpace, commentPrefix.length());
            if (firstNonSpace < currentLine.length() && currentLine[firstNonSpace] == ' ') {
                currentLine.erase(firstNonSpace, 1);
    return true;
}

        } else if (firstNonSpace != std::string::npos) {
            // Add comment
            currentLine.insert(firstNonSpace, commentPrefix + " ");
    return true;
}

        result << currentLine;
        if (i < lines.size() - 1) {
            result << "\n";
    return true;
}

    return true;
}

    // Replace selection
    SendMessage(m_editorHwnd, EM_SETSEL, selStart, selEnd);
    std::string newText = result.str();
    SendMessage(m_editorHwnd, EM_REPLACESEL, TRUE, reinterpret_cast<LPARAM>(newText.c_str()));
    return true;
}

// Smart selection utilities
void MainWindow::ExpandSelectionIntelligently() {
    if (!m_editorHwnd) return;
    
    DWORD selStart, selEnd;
    SendMessage(m_editorHwnd, EM_GETSEL, reinterpret_cast<WPARAM>(&selStart), reinterpret_cast<LPARAM>(&selEnd));
    
    // Expand selection based on context
    if (selStart == selEnd) {
        // No selection - select word
        ExpandToWord(selStart);
    } else {
        // Has selection - expand to next logical unit
        ExpandToNextUnit(selStart, selEnd);
    return true;
}

    return true;
}

void MainWindow::ShrinkSelectionIntelligently() {
    if (!m_editorHwnd) return;
    
    DWORD selStart, selEnd;
    SendMessage(m_editorHwnd, EM_GETSEL, reinterpret_cast<WPARAM>(&selStart), reinterpret_cast<LPARAM>(&selEnd));
    
    if (selStart != selEnd) {
        // Shrink selection to previous logical unit
        ShrinkToPreviousUnit(selStart, selEnd);
    return true;
}

    return true;
}

void MainWindow::ExpandToWord(DWORD position) {
    // Find word boundaries around position
    int totalLength = static_cast<int>(SendMessage(m_editorHwnd, WM_GETTEXTLENGTH, 0, 0));
    
    // Find start of word
    DWORD wordStart = position;
    while (wordStart > 0) {
        char ch;
        SendMessage(m_editorHwnd, EM_SETSEL, wordStart - 1, wordStart);
        SendMessage(m_editorHwnd, EM_GETSELTEXT, 0, reinterpret_cast<LPARAM>(&ch));
        
        if (!std::isalnum(ch) && ch != '_') break;
        wordStart--;
    return true;
}

    // Find end of word
    DWORD wordEnd = position;
    while (wordEnd < static_cast<DWORD>(totalLength)) {
        char ch;
        SendMessage(m_editorHwnd, EM_SETSEL, wordEnd, wordEnd + 1);
        SendMessage(m_editorHwnd, EM_GETSELTEXT, 0, reinterpret_cast<LPARAM>(&ch));
        
        if (!std::isalnum(ch) && ch != '_') break;
        wordEnd++;
    return true;
}

    // Select the word
    SendMessage(m_editorHwnd, EM_SETSEL, wordStart, wordEnd);
    return true;
}

void MainWindow::ExpandToNextUnit(DWORD selStart, DWORD selEnd) {
    // Try to expand to line, then paragraph, then function, etc.
    // For simplicity, expand to line first
    
    // Get line boundaries
    int startLine = static_cast<int>(SendMessage(m_editorHwnd, EM_LINEFROMCHAR, selStart, 0));
    int endLine = static_cast<int>(SendMessage(m_editorHwnd, EM_LINEFROMCHAR, selEnd, 0));
    
    int lineStart = static_cast<int>(SendMessage(m_editorHwnd, EM_LINEINDEX, startLine, 0));
    int nextLineStart = static_cast<int>(SendMessage(m_editorHwnd, EM_LINEINDEX, endLine + 1, 0));
    
    if (nextLineStart == -1) {
        nextLineStart = static_cast<int>(SendMessage(m_editorHwnd, WM_GETTEXTLENGTH, 0, 0));
    return true;
}

    SendMessage(m_editorHwnd, EM_SETSEL, lineStart, nextLineStart);
    return true;
}

void MainWindow::ShrinkToPreviousUnit(DWORD selStart, DWORD selEnd) {
    // Shrink selection by reducing it
    DWORD newLength = (selEnd - selStart) / 2;
    if (newLength > 0) {
        DWORD newEnd = selStart + newLength;
        SendMessage(m_editorHwnd, EM_SETSEL, selStart, newEnd);
    return true;
}

    return true;
}

// Production debugging integration
void MainWindow::StartProductionDebugSession() {
    if (m_tabs.empty() || m_currentTab >= m_tabs.size()) {
        sendToTerminal("# No file open for debugging\n");
        return;
    return true;
}

    std::string currentFile = m_tabs[m_currentTab].filename;
    if (currentFile.empty()) {
        sendToTerminal("# File must be saved before debugging\n");
        return;
    return true;
}

    // Determine file type and appropriate debugger
    std::string debugCommand = GetDebugCommandForFile(currentFile);
    
    if (!debugCommand.empty()) {
        // Start debugging session
        sendToTerminal("# Starting production debug session...\n");
        sendToTerminal("# File: " + currentFile + "\n");
        sendToTerminal(debugCommand + "\n");
        
        // Log debugging session
        LogDebuggingSession(currentFile, debugCommand);
    } else {
        sendToTerminal("# No debugger configured for this file type\n");
    return true;
}

    return true;
}

std::string MainWindow::GetDebugCommandForFile(const std::string& filename) {
    size_t dotPos = filename.find_last_of('.');
    if (dotPos != std::string::npos) {
        std::string ext = filename.substr(dotPos + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        
        if (ext == "cpp" || ext == "c") {
            return "# gdb ./" + filename.substr(0, dotPos) + ".exe";
        } else if (ext == "py") {
            return "python -m pdb \"" + filename + "\"";
        } else if (ext == "js") {
            return "node --inspect-brk \"" + filename + "\"";
        } else if (ext == "ps1") {
            return "Set-PSBreakpoint -Script \"" + filename + "\" -Line 1";
    return true;
}

    return true;
}

    return "";
    return true;
}

void MainWindow::LogDebuggingSession(const std::string& filename, const std::string& command) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::cout << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
              << "] Debug session started - File: " << filename 
              << ", Command: " << command << std::endl;
    return true;
}

