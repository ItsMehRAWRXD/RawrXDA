
!function(){try{var e="undefined"!=typeof window?window:"undefined"!=typeof global?global:"undefined"!=typeof self?self:{},n=(new e.Error).stack;n&&(e._sentryDebugIds=e._sentryDebugIds||{},e._sentryDebugIds[n]="ac9a46a2-cb92-5e4d-83e1-783117d45f31")}catch(e){}}();
(function(){"use strict";const{ipcRenderer:n,contextBridge:u,webFrame:a}=require("electron"),d=[".okta.com",".okta-emea.com",".oktapreview.com",".duosecurity.com",".duo.com",".login.microsoftonline.com",".onelogin.com",".auth0.com",".pingidentity.com",".pingone.com"];function g(){try{const e=window.location.hostname.toLowerCase();if(!d.some(t=>e===t.slice(1)||e.endsWith(t)))return;a.executeJavaScript(`
				(function() {
					if (typeof navigator === 'undefined' || !navigator.permissions || !navigator.permissions.query) {
						return;
					}
					if (navigator.permissions.__localNetworkPolyfillApplied) {
						return;
					}
					navigator.permissions.__localNetworkPolyfillApplied = true;

					const originalQuery = navigator.permissions.query.bind(navigator.permissions);
					navigator.permissions.query = async function(descriptor) {
						if (descriptor && (descriptor.name === 'local-network-access' || descriptor.name === 'local-network')) {
							return {
								state: 'granted',
								name: descriptor.name,
								onchange: null,
								addEventListener: function() {},
								removeEventListener: function() {},
								dispatchEvent: function() { return true; }
							};
						}
						return originalQuery(descriptor);
					};
				})();
			`)}catch(e){console.error("[WebviewBrowser Preload] Failed to inject local network access polyfill:",e)}}g();const c=process.platform==="darwin",f={send:(e,...r)=>{["focus-url-bar","element-selected","element-updated","keyboard-shortcut","area-screenshot-selected","style-changes-confirmed","css-inspector-style-change","open-url-side-group","open-url-new-tab","focus-composer-input","css-inspector-undo","css-inspector-redo","show-dialog","show-dialog-dummy"].includes(e)&&n.sendToHost(e,...r)}};try{u.exposeInMainWorld("cursorBrowser",f)}catch(e){console.error("[WebviewBrowser Preload] Failed to expose bridge:",e)}function l(){const e=`
			(function() {
				if (window.__cursorDialogOverridesApplied) {
					return;
				}
				window.__cursorDialogOverridesApplied = true;

				window.__cursorDialogConfig = {
					confirmResult: true,
					promptResult: null,
					dialogHistory: []
				};

				window.__cursorSetDialogConfig = function(config) {
					if (typeof config.confirmResult === 'boolean') {
						window.__cursorDialogConfig.confirmResult = config.confirmResult;
					}
					if (config.promptResult !== undefined) {
						window.__cursorDialogConfig.promptResult = config.promptResult;
					}
				};

				window.__cursorGetDialogHistory = function() {
					return window.__cursorDialogConfig.dialogHistory.slice();
				};

				window.__cursorClearDialogHistory = function() {
					window.__cursorDialogConfig.dialogHistory = [];
				};

				window.alert = function(message) {
					const msgStr = String(message ?? '');
					console.log('[CursorBrowser] Dialog suppressed: alert - ' + msgStr);
					window.__cursorDialogConfig.dialogHistory.push({ type: 'alert', message: msgStr, timestamp: Date.now() });
					return undefined;
				};

				window.confirm = function(message) {
					const msgStr = String(message ?? '');
					const result = window.__cursorDialogConfig.confirmResult;
					console.log('[CursorBrowser] Dialog suppressed: confirm - ' + msgStr + ' (returning ' + result + ')');
					window.__cursorDialogConfig.dialogHistory.push({ type: 'confirm', message: msgStr, result: result, timestamp: Date.now() });
					return result;
				};

				window.prompt = function(message, defaultValue) {
					const msgStr = String(message ?? '');
					const defVal = defaultValue ?? '';
					const configuredResult = window.__cursorDialogConfig.promptResult;
					const result = configuredResult !== null ? configuredResult : defVal;
					console.log('[CursorBrowser] Dialog suppressed: prompt - ' + msgStr + ' (returning: ' + result + ')');
					window.__cursorDialogConfig.dialogHistory.push({ type: 'prompt', message: msgStr, defaultValue: defVal, result: result, timestamp: Date.now() });
					return result;
				};

				console.log('[CursorBrowser] Native dialog overrides installed - dialogs are now non-blocking');
			})();
		`;try{a.executeJavaScript(e)}catch(r){console.error("[WebviewBrowser Preload] Failed to inject early dialog overrides:",r)}}l(),window.addEventListener("DOMContentLoaded",()=>{l(),document.addEventListener("click",e=>{if(!e.altKey)return;const r=e.target.closest("a[href]");if(!r)return;const s=r.href;!s||s.startsWith("javascript:")||(e.preventDefault(),e.stopPropagation(),n.sendToHost("open-url-side-group",{url:s}))},!0)}),document.addEventListener("keydown",e=>{if(!e.isTrusted)return;const r=c?e.metaKey:e.ctrlKey,s=e.shiftKey,t=e.altKey,i=e.key.toLowerCase();let o;if(r&&!s&&!t)switch(i){case"r":o="reload-page";break;case"l":o="focus-url-bar";break;case"t":o="new-browser-tab";break;case"i":o="focus-composer";break;case"b":o="toggle-sidebar";break;case"w":o="close-browser-tab";break;case"=":case"+":o="zoom-in";break;case"-":o="zoom-out";break;case"0":o="zoom-reset";break;case"z":o="undo";break;case"a":o="select-all";break;case"c":o="copy";break;case"v":o="paste";break;case"x":o="cut";break}if(r&&s&&!t)switch(i){case"i":o="open-devtools";break;case"z":o="redo";break}if(t&&!r&&!s)switch(i){case"arrowleft":o="navigate-back";break;case"arrowright":o="navigate-forward";break}if(!r&&!s&&!t)switch(i){case"f5":o="reload-page";break;case"f12":o="open-devtools";break}if(c&&e.metaKey&&e.altKey&&!s)switch(i){case"i":case"c":case"j":o="open-devtools";break}o&&(e.preventDefault(),n.sendToHost("keyboard-shortcut",{shortcut:o})),n.sendToHost("did-keydown",{key:e.key,keyCode:e.keyCode,code:e.code,shiftKey:e.shiftKey,altKey:e.altKey,ctrlKey:e.ctrlKey,metaKey:e.metaKey,repeat:e.repeat})},!0)})();

//# sourceMappingURL=http://go/sourcemap/sourcemaps/dc8361355d709f306d5159635a677a571b277bc0/core/vs/workbench/contrib/composer/browser/preload-webview-browser.js.map

//# debugId=ac9a46a2-cb92-5e4d-83e1-783117d45f31
