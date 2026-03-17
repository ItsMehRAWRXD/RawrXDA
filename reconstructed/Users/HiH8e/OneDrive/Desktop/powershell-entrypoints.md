# PowerShell Entrypoints (Mapped)

Primary IDE/Browser launchers:
- Mirai-Source-Code-master/Launch-Modern-IDE.ps1
- Mirai-Source-Code-master/launch-beast-browser.ps1
- START-FULL-IDE.ps1 (root Desktop)
- Beast-IDEBrowser.ps1 (Mirai-Source-Code-master)

Diagnostic / Fix scripts (automation, can be ported later):
- fix-dom-errors.ps1
- fix-domready-function.ps1
- fix-js-syntax-errors.ps1
- cleanup-duplicate-handlers.ps1
- comprehensive-ide-fix.ps1
- final-ide-verification.ps1
- verify-js-fixes.ps1
- test-dom-fixes.ps1

Build / native helpers:
- dlr/build_windows.ps1

Model compression helpers:
- scripts/compress_model.ps1
- scripts/decompress_model.ps1

Misc / experimental:
- nuclear-handler-cleanup.ps1
- bearer-proxy.ps1
- beforePromptSubmit.ps1
- ssadwqhaq.ps1

Next step: Replace launch scripts with a single cross-platform Node launcher (`ide-launcher.js`).