/**
 * RawrXD v14.7.0 ModulesPanel.js
 * High-performance binary-to-DOM renderer for the PEB Module List
 */
class ModulesPanel {
  constructor(containerId) {
    this.container = document.getElementById(containerId);
    this.moduleList = [];
    this.rowHeight = 24;
    this.visibleRows = 30; // Approx visible area
    this.scrollTop = 0;

    // Virtual scroll setup
    this.container.classList.add('module-list-container');
    this.container.addEventListener('scroll', (e) => {
      this.scrollTop = e.target.scrollTop;
      this.render();
    });
  }

  updateFromSnapshot(arrayBuffer) {
    const view = new DataView(arrayBuffer);
    const totalModules = view.getUint32(0, true);
    let offset = 4;
    const newModules = [];

    for (let i = 0; i < totalModules; i++) {
      if (offset >= arrayBuffer.byteLength) break;

      const base = view.getBigUint64(offset, true);
      const size = view.getBigUint64(offset + 8, true);
      const pathLen = view.getUint32(offset + 16, true);
      offset += 20;

      const pathBytes = new Uint8Array(arrayBuffer, offset, pathLen);
      const path = new TextDecoder().decode(pathBytes);
      offset += pathLen;

      newModules.push({ base, size, path });
    }

    this.moduleList = newModules;
    this.render();
  }

  render() {
    const totalHeight = this.moduleList.length * this.rowHeight;
    const startIndex = Math.floor(this.scrollTop / this.rowHeight);
    const endIndex = Math.min(startIndex + this.visibleRows, this.moduleList.length);

    const visibleItems = this.moduleList.slice(startIndex, endIndex).map((m, i) => {
      const top = (startIndex + i) * this.rowHeight;
      return `
                <div class="module-row" style="top: ${top}px" onclick="window.rawrxd.inspectModule('${m.base.toString(16)}')">
                    <div class="module-cell" style="width: 150px">0x${m.base.toString(16).toUpperCase()}</div>
                    <div class="module-cell" style="width: 80px">${(Number(m.size) / 1024).toFixed(0)} KB</div>
                    <div class="module-cell">${m.path}</div>
                </div>
            `;
    }).join('');

    this.container.innerHTML = `
            <div class="module-header">
                <div style="width: 150px">Base Address</div>
                <div style="width: 80px">Size</div>
                <div>Module Path</div>
            </div>
            <div class="module-virtual-scroll" style="height: ${totalHeight}px">
                ${visibleItems}
            </div>
        `;
  }
}

// Global hook for the WebView2 IPC Bridge
window.onRawrIPCData = (msgType, payload) => {
  if (msgType === 0x2001) { // MOD_LOAD snapshot
    if (!window.modulesPanel) window.modulesPanel = new ModulesPanel('modules-container');
    window.modulesPanel.updateFromSnapshot(payload);
  }
};
