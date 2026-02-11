"use strict";
!function(){try{var e="undefined"!=typeof window?window:"undefined"!=typeof global?global:"undefined"!=typeof self?self:{},n=(new e.Error).stack;n&&(e._sentryDebugIds=e._sentryDebugIds||{},e._sentryDebugIds[n]="2eb38b08-cd83-50bd-88b9-6417563982a5")}catch(e){}}();
(function(){const{ipcRenderer:o,webFrame:n,contextBridge:s}=require("electron");function t(e){if(!e||!e.startsWith("vscode:"))throw new Error(`Unsupported event IPC channel '${e}'`);return!0}const i={ipcRenderer:{send(e,...r){t(e)&&o.send(e,...r)},invoke(e,...r){return t(e),o.invoke(e,...r)}},webFrame:{setZoomLevel(e){typeof e=="number"&&n.setZoomLevel(e)}}};try{s.exposeInMainWorld("vscode",i)}catch(e){console.error(e)}})();

//# sourceMappingURL=http://go/sourcemap/sourcemaps/dc8361355d709f306d5159635a677a571b277bc0/core/vs/base/parts/sandbox/electron-sandbox/preload-aux.js.map

//# debugId=2eb38b08-cd83-50bd-88b9-6417563982a5

