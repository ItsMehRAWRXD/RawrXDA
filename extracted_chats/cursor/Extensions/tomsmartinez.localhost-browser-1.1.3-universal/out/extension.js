"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.deactivate = exports.activate = void 0;
const vscode = require("vscode");
const fs = require("node:fs");
const path = require("node:path");
function activate(context) {
    // Status bar item
    const statusBarItem = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Right, 100);
    statusBarItem.text = "$(browser) Open Localhost";
    statusBarItem.tooltip = "Click to open localhost in Simple Browser";
    statusBarItem.command = "localhostBrowser.openBrowser";
    statusBarItem.show();
    // Command registration
    const disposable = vscode.commands.registerCommand("localhostBrowser.openBrowser", async () => {
        const port = await findPort();
        const url = port ? `http://localhost:${port}` : "http://localhost";
        // Open in Simple Browser
        await vscode.commands.executeCommand("simpleBrowser.show", url);
        // Status message
        vscode.window.setStatusBarMessage(`Opened ${url} in Simple Browser`, 3000);
    });
    context.subscriptions.push(disposable, statusBarItem);
}
exports.activate = activate;
async function findPort() {
    const folders = vscode.workspace.workspaceFolders;
    if (!folders)
        return null;
    const root = folders[0].uri.fsPath;
    const config = vscode.workspace.getConfiguration("localhostBrowser");
    // Get user/workspace setting
    let searchFiles = config.get("searchPaths");
    // If no setting exists, use default array
    if (!searchFiles || searchFiles.length === 0) {
        searchFiles = [
            "mkdocs.yml",
            "mkdocs.yaml",
            ".env",
            ".env.local",
            ".env.development",
            "package.json",
            "docker-compose.yml",
            "docker-compose.yaml",
            "Dockerfile",
            "next.config.js",
            "vite.config.js",
            "vite.config.ts",
            "angular.json",
            "vue.config.js",
        ];
    }
    // Convert to Set to remove duplicates (optional)
    searchFiles = Array.from(new Set(searchFiles));
    for (const fileName of searchFiles) {
        const filePath = path.join(root, fileName);
        // console.log("checking file", fileName, filePath);
        if (!fs.existsSync(filePath))
            continue;
        try {
            const content = fs.readFileSync(filePath, "utf8");
            const port = extractPort(content, fileName);
            if (port)
                return port;
        }
        catch {
            // ignore read errors
        }
    }
    // Fallback to common development ports
    const commonPorts = [
        "3000",
        "3001",
        "4200",
        "5000",
        "5173",
        "8000",
        "8080",
        "8081",
        "9000",
    ];
    for (const port of commonPorts) {
        if (await isPortInUse(port))
            return port;
    }
    return null;
}
function extractPort(content, fileName) {
    // console.log("CONTENT", fileName);
    // MkDocs Material default port
    if (fileName === "mkdocs.yml")
        return "8000";
    // .env files
    if (fileName.startsWith(".env")) {
        const match = content.match(/^PORT\s*=\s*(\d+)/m);
        if (match)
            return match[1];
    }
    // package.json scripts
    if (fileName === "package.json") {
        try {
            const pkg = JSON.parse(content);
            if (pkg.scripts) {
                for (const script of Object.values(pkg.scripts)) {
                    const match = script.match(/(?:--port|--port=|-p)\s*(\d+)/);
                    if (match)
                        return match[1];
                }
            }
        }
        catch { }
    }
    // docker-compose
    if (fileName.includes("docker-compose")) {
        const match = content.match(/ports:\s*\n\s*-\s*["']?(\d+):/m);
        if (match)
            return match[1];
    }
    // config files (Next.js, Vite, etc.)
    if (fileName.includes("config")) {
        const match = content.match(/port:\s*(\d+)/);
        if (match)
            return match[1];
    }
    // Generic PORT
    const genericMatch = content.match(/PORT\s*[:=]\s*(\d+)/i);
    return genericMatch ? genericMatch[1] : null;
}
async function isPortInUse(port) {
    // Simplified placeholder
    return false;
}
function deactivate() { }
exports.deactivate = deactivate;
//# sourceMappingURL=extension.js.map