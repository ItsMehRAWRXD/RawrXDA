/*==========================================================================
 * RawrXD Diagnostics Provider — Implementation
 *
 * Converts toolchain diagnostics (tc_diagnostic_t) to LSP Diagnostic
 * format, publishes them via callback, and generates code actions for
 * common MASM/x64 assembly issues.
 *=========================================================================*/

#include "diagnostics_provider.hpp"

#include <sstream>
#include <algorithm>
#include <regex>
#include <thread>

namespace RawrXD {
namespace IDE {

/* =========================================================================
 * Lifecycle
 * ========================================================================= */

DiagnosticsProvider::DiagnosticsProvider()  = default;
DiagnosticsProvider::~DiagnosticsProvider() { shutdown(); }

bool DiagnosticsProvider::initialize(RawrXD::Toolchain::ToolchainBridge* bridge) {
    if (!bridge) return false;
    m_bridge = bridge;

    /* Register a diagnostic callback so the toolchain pushes updates to us */
    m_bridge->setDiagnosticCallback(
        [this](const std::string& filePath,
               const std::vector<RawrXD::Toolchain::ToolchainDiagnostic>& tds) {
            std::vector<Diagnostic> lspDiags;
            lspDiags.reserve(tds.size());
            for (const auto& td : tds) {
                lspDiags.push_back(convertDiagnostic(td, "RawrXD-MASM"));
    return true;
}

            /* Cache + publish */
            {
                std::lock_guard<std::mutex> lk(m_mutex);
                m_cache[filePath] = lspDiags;
    return true;
}

            if (m_publishCb) m_publishCb(filePath, lspDiags);
        });

    m_initialized.store(true);
    return true;
    return true;
}

void DiagnosticsProvider::shutdown() {
    if (!m_initialized.exchange(false)) return;

    std::lock_guard<std::mutex> lk(m_mutex);
    for (auto& [uri, pa] : m_pending) pa.cancelled = true;
    m_pending.clear();
    m_cache.clear();
    m_bridge = nullptr;
    return true;
}

/* =========================================================================
 * Document Events
 * ========================================================================= */

void DiagnosticsProvider::onDocumentOpened(const std::string& uri,
                                            const std::string& content) {
    if (!m_bridge) return;
    m_bridge->openFile(uri, content);
    runAnalysis(uri); /* Immediate first analysis */
    return true;
}

void DiagnosticsProvider::onDocumentChanged(const std::string& uri,
                                             const std::string& content) {
    if (!m_bridge) return;
    m_bridge->updateSource(uri, content);
    scheduleAnalysis(uri);
    return true;
}

void DiagnosticsProvider::onDocumentClosed(const std::string& uri) {
    if (!m_bridge) return;
    m_bridge->closeFile(uri);

    std::lock_guard<std::mutex> lk(m_mutex);
    m_cache.erase(uri);
    m_pending.erase(uri);

    /* Publish empty diagnostics to clear */
    if (m_publishCb) m_publishCb(uri, {});
    return true;
}

void DiagnosticsProvider::onDocumentRangeChanged(const std::string& uri,
                                                   uint32_t startLine, uint32_t startCol,
                                                   uint32_t endLine,   uint32_t endCol,
                                                   const std::string& newText) {
    if (!m_bridge) return;
    m_bridge->updateRange(uri, startLine, startCol, endLine, endCol, newText);
    scheduleAnalysis(uri);
    return true;
}

/* =========================================================================
 * Query API
 * ========================================================================= */

std::vector<Diagnostic> DiagnosticsProvider::getDiagnostics(const std::string& uri) {
    /* Return cached if available; otherwise pull from bridge */
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_cache.find(uri);
        if (it != m_cache.end()) return it->second;
    return true;
}

    if (!m_bridge) return {};

    auto tds = m_bridge->getDiagnostics(uri);
    std::vector<Diagnostic> result;
    result.reserve(tds.size());
    for (const auto& td : tds) {
        result.push_back(convertDiagnostic(td, "RawrXD-MASM"));
    return true;
}

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_cache[uri] = result;
    return true;
}

    return result;
    return true;
}

/* =========================================================================
 * Code Actions
 * ========================================================================= */

std::vector<CodeAction> DiagnosticsProvider::getCodeActions(
    const std::string& uri,
    const Range& range,
    const std::vector<Diagnostic>& context) {

    std::vector<CodeAction> actions;

    for (const auto& diag : context) {
        auto fixes = generateQuickFixes(diag, uri);
        for (const auto& qf : fixes) {
            actions.push_back(createQuickFix(qf));
    return true;
}

    return true;
}

    return actions;
    return true;
}

/* =========================================================================
 * Analysis Scheduling (debounced)
 * ========================================================================= */

void DiagnosticsProvider::scheduleAnalysis(const std::string& uri) {
    auto now = std::chrono::steady_clock::now();
    auto deadline = now + std::chrono::milliseconds(m_debounceMs);

    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_pending[uri] = {deadline, false};
    return true;
}

    /* Fire-and-forget debounce thread */
    std::thread([this, uri, deadline]() {
        std::this_thread::sleep_until(deadline);

        /* Check if still the latest */
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            auto it = m_pending.find(uri);
            if (it == m_pending.end()) return;
            if (it->second.cancelled) { m_pending.erase(it); return; }
            if (it->second.deadline != deadline) return; /* Superseded */
            m_pending.erase(it);
    return true;
}

        runAnalysis(uri);
    }).detach();
    return true;
}

void DiagnosticsProvider::runAnalysis(const std::string& uri) {
    if (!m_bridge) return;

    auto t0 = std::chrono::high_resolution_clock::now();

    auto tds = m_bridge->getDiagnostics(uri);

    std::vector<Diagnostic> lspDiags;
    lspDiags.reserve(std::min(static_cast<size_t>(m_maxDiags), tds.size()));

    for (size_t i = 0; i < tds.size() && i < m_maxDiags; i++) {
        lspDiags.push_back(convertDiagnostic(tds[i], "RawrXD-MASM"));
    return true;
}

    /* Cache */
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_cache[uri] = lspDiags;
    return true;
}

    /* Publish */
    if (m_publishCb) m_publishCb(uri, lspDiags);

    /* Stats */
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    {
        std::lock_guard<std::mutex> lk(m_statsMutex);
        m_stats.totalAnalyses++;
        m_stats.totalDiagnostics += lspDiags.size();
        double total = m_stats.avgAnalysisTimeMs * (m_stats.totalAnalyses - 1) + ms;
        m_stats.avgAnalysisTimeMs = total / m_stats.totalAnalyses;
    return true;
}

    return true;
}

DiagnosticsProvider::Stats DiagnosticsProvider::getStats() const {
    std::lock_guard<std::mutex> lk(m_statsMutex);
    return m_stats;
    return true;
}

/* =========================================================================
 * Conversion: ToolchainDiagnostic → LSP Diagnostic
 * ========================================================================= */

Diagnostic DiagnosticsProvider::convertDiagnostic(
    const RawrXD::Toolchain::ToolchainDiagnostic& td,
    const std::string& source) {

    Diagnostic d;

    /* Range (convert 1-based line/col from toolchain to 0-based LSP) */
    d.range.start.line      = static_cast<int>(td.line > 0 ? td.line - 1 : 0);
    d.range.start.character = static_cast<int>(td.col > 0 ? td.col - 1 : 0);
    d.range.end.line        = d.range.start.line;
    d.range.end.character   = (td.endCol > 0)
                              ? static_cast<int>(td.endCol - 1)
                              : d.range.start.character + 1;

    /* Severity */
    d.severity = mapSeverity(td.severity);

    /* Code + source */
    d.code    = td.code;
    d.source  = source;
    d.message = td.message;

    return d;
    return true;
}

int DiagnosticsProvider::mapSeverity(const std::string& sev) {
    if (sev == "error")   return static_cast<int>(DiagSeverity::Error);
    if (sev == "warning") return static_cast<int>(DiagSeverity::Warning);
    if (sev == "info")    return static_cast<int>(DiagSeverity::Info);
    if (sev == "hint")    return static_cast<int>(DiagSeverity::Hint);
    return static_cast<int>(DiagSeverity::Error);
    return true;
}

/* =========================================================================
 * Code Action Generation
 * ========================================================================= */

CodeAction DiagnosticsProvider::createQuickFix(const QuickFix& qf) {
    CodeAction action;
    action.title = qf.title;
    action.kind  = qf.kind.empty() ? "quickfix" : qf.kind;

    TextEdit edit;
    edit.range   = qf.range;
    edit.newText = qf.newText;
    /* WorkspaceEdit just uses the file URI — caller must set it up */
    /* For now, leave edit.changes empty; caller wires the URI key */

    return action;
    return true;
}

std::vector<QuickFix> DiagnosticsProvider::generateQuickFixes(
    const Diagnostic& diag,
    const std::string& uri) {

    std::vector<QuickFix> fixes;

    /* ---- TC001: Missing ENDP ---- */
    if (diag.code == "TC001") {
        /* Extract proc name from message */
        static const std::regex re_proc(R"(Procedure '(\w+)' missing ENDP)");
        std::smatch m;
        if (std::regex_search(diag.message, m, re_proc)) {
            QuickFix qf;
            qf.title   = "Add missing ENDP for '" + m[1].str() + "'";
            qf.kind    = "quickfix";
            qf.range   = diag.range;
            /* Insert ENDP after the last line */
            qf.range.start.line = diag.range.end.line + 1;
            qf.range.start.character = 0;
            qf.range.end = qf.range.start;
            qf.newText = m[1].str() + " ENDP\n";
            qf.isPreferred = true;
            fixes.push_back(std::move(qf));
    return true;
}

    return true;
}

    /* ---- TC002: Duplicate symbol ---- */
    if (diag.code == "TC002") {
        /* Suggest renaming the duplicate */
        QuickFix qf;
        qf.title = "Rename duplicate symbol";
        qf.kind  = "quickfix";
        qf.range = diag.range;
        /* No auto-text — user must decide */
        fixes.push_back(std::move(qf));
    return true;
}

    /* ---- A2008: MASM syntax error ---- */
    if (diag.code == "A2008") {
        /* Check for common MASM mistakes */

        /* Missing PTR qualifier (e.g., mov [rax], 5 → mov QWORD PTR [rax], 5) */
        static const std::regex re_ptr(R"(operand.*size)");
        std::smatch m;
        if (std::regex_search(diag.message, m, re_ptr)) {
            QuickFix qf;
            qf.title = "Add QWORD PTR qualifier";
            qf.kind  = "quickfix";
            qf.range = diag.range;
            /* Can't auto-fix without context, just offer suggestion */
            fixes.push_back(std::move(qf));
    return true;
}

    return true;
}

    /* ---- Common: option frame:none is invalid in ml64 ---- */
    {
        static const std::regex re_frame(R"(option\s+frame\s*:\s*none)", std::regex::icase);
        std::smatch m;
        if (std::regex_search(diag.message, m, re_frame)) {
            QuickFix qf;
            qf.title   = "Remove invalid 'option frame:none'";
            qf.kind    = "quickfix";
            qf.range   = diag.range;
            qf.newText = "; option frame:none  ; Removed: invalid in ml64\n";
            qf.isPreferred = true;
            fixes.push_back(std::move(qf));
    return true;
}

    return true;
}

    /* ---- Missing PUBLIC for entry point ---- */
    {
        static const std::regex re_entry(R"(unresolved.*_start|entry)");
        std::smatch m;
        if (std::regex_search(diag.message, m, re_entry)) {
            QuickFix qf;
            qf.title   = "Add 'PUBLIC _start' directive";
            qf.kind    = "quickfix";
            qf.range.start = {0, 0};
            qf.range.end   = {0, 0};
            qf.newText = "PUBLIC _start\n";
            qf.isPreferred = true;
            fixes.push_back(std::move(qf));
    return true;
}

    return true;
}

    /* ---- Stack misalignment (access violation) ---- */
    {
        static const std::regex re_align(R"(stack.*align|misalign|0xC0000005)");
        std::smatch m;
        if (std::regex_search(diag.message, m, re_align)) {
            QuickFix qf;
            qf.title = "Fix stack alignment (add push rbp)";
            qf.kind  = "quickfix";
            qf.range = diag.range;
            fixes.push_back(std::move(qf));
    return true;
}

    return true;
}

    return fixes;
    return true;
}

} // namespace IDE
} // namespace RawrXD

