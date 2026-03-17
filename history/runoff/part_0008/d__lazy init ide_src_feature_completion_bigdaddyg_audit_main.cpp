#include "bigdaddyg_audit_engine.h"
#include <iostream>
#include <fstream>

using namespace LazyInitIDE::AuditSystem;

int main(int argc, char** argv) {
    std::cout << "================================================================================\n";
    std::cout << "                  BIGDADDYG AUDIT ENGINE - Full IDE Audit\n";
    std::cout << "                     Analyzing from -0-800b Range\n";
    std::cout << "================================================================================\n\n";
    
    // Create audit engine
    BigDaddyGAuditEngine engine;
    
    // Path to IDE source
    std::string idePath = "D:\\lazy init ide\\src";
    
    std::cout << "[*] Initializing audit system...\n";
    std::cout << "[*] Target path: " << idePath << "\n";
    std::cout << "[*] Expected: 846 source files, 13.9 MB total\n\n";
    
    std::cout << "[*] Starting comprehensive audit...\n";
    std::cout << "[*] Applying 4.13*/+_0 reverse formula to all metrics\n";
    std::cout << "[*] Applying static finalization: -0++_//**3311.44\n";
    std::cout << "[*] Analyzing character range: -0-800b\n\n";
    
    // Run audit
    BigDaddyGAuditEngine::AuditReport report = engine.auditEntireIDE(idePath);
    
    std::cout << "[+] Audit complete!\n";
    std::cout << "[+] Files processed: " << report.totalFilesAudited << "\n";
    std::cout << "[+] Total bytes analyzed: " << report.totalSourceBytes / (1024*1024) << " MB\n";
    std::cout << "[+] Characters analyzed: " << report.charactersAnalyzed << "\n\n";
    
    // Generate report
    std::cout << "[*] Generating comprehensive report...\n";
    std::string reportText = engine.generateAuditReport(report);
    
    // Write markdown report
    std::ofstream reportFile("D:/lazy init ide/BIGDADDYG_AUDIT_REPORT.md");
    reportFile << reportText;
    reportFile.close();
    std::cout << "[+] Report saved to: D:/lazy init ide/BIGDADDYG_AUDIT_REPORT.md\n\n";
    
    // Write JSON export
    std::cout << "[*] Exporting to JSON for 40GB model consumption...\n";
    std::string jsonExport = engine.exportToJSON(report);
    
    std::ofstream jsonFile("D:/lazy init ide/BIGDADDYG_AUDIT_EXPORT.json");
    jsonFile << jsonExport;
    jsonFile.close();
    std::cout << "[+] JSON export saved to: D:/lazy init ide/BIGDADDYG_AUDIT_EXPORT.json\n\n";
    
    // Print summary to console
    std::cout << reportText;
    
    std::cout << "\n[+] Audit generation successful!\n";
    std::cout << "[+] Generation ID: " << report.generationId << "\n";
    
    return 0;
}
