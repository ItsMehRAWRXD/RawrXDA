
!function(){try{var e="undefined"!=typeof window?window:"undefined"!=typeof global?global:"undefined"!=typeof self?self:{},n=(new e.Error).stack;n&&(e._sentryDebugIds=e._sentryDebugIds||{},e._sentryDebugIds[n]="62430a14-7d8c-5014-ab68-58018771d943")}catch(e){}}();
(function(){"use strict";const{ipcRenderer:t,contextBridge:s,webFrame:n}=require("electron"),c=[".okta.com",".okta-emea.com",".oktapreview.com",".duosecurity.com",".duo.com",".login.microsoftonline.com",".onelogin.com",".auth0.com",".pingidentity.com",".pingone.com"];function a(){try{const e=window.location.hostname.toLowerCase();if(!c.some(i=>e===i.slice(1)||e.endsWith(i)))return;n.executeJavaScript(`
				(function() {
					if (typeof navigator === 'undefined' || !navigator.permissions || !navigator.permissions.query) {
						return;
					}
					// Avoid double-patching
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
			`)}catch(e){console.error("[BrowserView Preload] Failed to inject local network access polyfill:",e)}}a();const l={send:(e,...r)=>{(e==="focus-url-bar"||e==="element-selected"||e==="element-updated"||e==="keyboard-shortcut"||e==="area-screenshot-selected"||e==="style-changes-confirmed"||e==="css-inspector-style-change")&&t.send("vscode:browser-view-message",e,...r)}};try{s.exposeInMainWorld("cursorBrowser",l)}catch(e){console.error("[BrowserView Preload] Failed to expose bridge:",e)}window.addEventListener("DOMContentLoaded",()=>{document.addEventListener("click",e=>{if(!e.altKey)return;const r=e.target.closest("a[href]");if(!r)return;const o=r.href;!o||o.startsWith("javascript:")||(e.preventDefault(),e.stopPropagation(),t.send("vscode:browser-view-message","open-url-side-group",{url:o}))},!0)})})();

//# sourceMappingURL=http://go/sourcemap/sourcemaps/dc8361355d709f306d5159635a677a571b277bc0/core/vs/platform/browserView/electron-main/preload-browser.js.map

//# debugId=62430a14-7d8c-5014-ab68-58018771d943

