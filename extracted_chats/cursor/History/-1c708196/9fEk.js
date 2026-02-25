import { registerCommand } from "./command-registry.js";
import { mountSidebar } from "./sidebar.js";
import { initWebGL } from "./webgl.js";
import { initSecuritySuite } from "./security.js";
import { initCompilerPanel } from "./compiler.js";

export function registerDefaultCommands() {
    registerCommand("ide.reloadSidebar", {
        title: "Reload File Explorer",
        category: "Workspace",
        shortcut: "Ctrl+Shift+R",
        run: () => mountSidebar()
    });

    registerCommand("ide.toggleWebGL", {
        title: "Toggle WebGL Panel",
        category: "View",
        run: () => {
            const panel = document.getElementById("webgl-panel");
            panel.classList.toggle("hidden");
        }
    });

    registerCommand("ide.resetWebGL", {
        title: "Reset WebGL Pipeline",
        category: "Game Dev",
        run: () => initWebGL(true)
    });

    registerCommand("ide.runSecurityAudit", {
        title: "Run Security Audit",
        category: "Security",
        run: () => initSecuritySuite(true)
    });

    registerCommand("ide.runCompilerPipeline", {
        title: "Run Compiler Pipeline",
        category: "Compiler",
        run: () => initCompilerPanel(true)
    });
}

