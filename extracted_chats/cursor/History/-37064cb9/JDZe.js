/**
 * Bottom Panel Manager
 * Unified dock for terminal, console, browser, etc.
 */

(function() {
'use strict';

class BottomPanelManager {
    constructor() {
        this.container = document.getElementById('bottom-panel');
        this.tabsContainer = document.getElementById('bottom-panel-tabs');
        this.viewsContainer = document.getElementById('bottom-panel-views');

        if (!this.container || !this.tabsContainer || !this.viewsContainer) {
            console.warn('[BottomPanel] ⚠️ Bottom panel containers missing');
            return;
        }

        this.panels = new Map();
        this.activePanel = null;
        this.isCollapsed = true;
        this.height = this.loadHeight();
        this.minHeight = 220;
        this.defaultHeight = 320;
        this.maximized = false;

        this.restoreState();
        this.applyHeight(this.height);
        this.createControls();
        this.registerResizeObserver();
        window.addEventListener('resize', () => this.applyHeight(this.height));
    }

    registerPanel(id, options = {}) {
        if (!id) {
            console.warn('[BottomPanel] ⚠️ Panel requires an id');
            return;
        }

        const defaultOptions = {
            title: 'Panel',
            icon: '📟',
            element: null,
            onShow: () => {},
            onHide: () => {},
            defaultVisible: false,
        };

        const panelOptions = { ...defaultOptions, ...options };

        if (!panelOptions.element) {
            panelOptions.element = this.viewsContainer.querySelector(`[data-panel-id="${id}"]`);
        }

        if (!panelOptions.element) {
            console.warn(`[BottomPanel] ⚠️ Element not found for panel "${id}"`);
            return;
        }

        panelOptions.element.classList.add('bottom-view');
        panelOptions.element.dataset.panelId = id;

        const tabButton = this.createTabButton(id, panelOptions.title, panelOptions.icon);
        this.tabsContainer.appendChild(tabButton);

        this.panels.set(id, {
            ...panelOptions,
            tab: tabButton,
        });

        if (!this.isCollapsed && this.activePanel === id) {
            this.showPanel(id, { focusOnly: true });
        } else if (panelOptions.defaultVisible || (!this.activePanel && !this.isCollapsed)) {
            this.showPanel(id, { focusOnly: !panelOptions.defaultVisible });
        } else {
            this.hidePanelContent(id);
        }
    }

    createTabButton(id, title, icon) {
        const button = document.createElement('button');
        button.className = 'bottom-tab';
        button.dataset.panelId = id;
        button.type = 'button';
        button.innerHTML = `<span>${icon}</span><span>${title}</span>`;
        button.addEventListener('click', () => this.togglePanel(id));
        return button;
    }

    togglePanel(id) {
        if (this.isCollapsed || this.activePanel !== id) {
            this.showPanel(id);
        } else {
            this.collapse();
        }
    }

    showPanel(id, { focusOnly = false } = {}) {
        const panel = this.panels.get(id);
        if (!panel) return;

        this.ensureExpanded();

        if (this.activePanel && this.activePanel !== id) {
            this.hidePanelContent(this.activePanel);
        }

        this.activePanel = id;
        this.panels.forEach((registeredPanel, panelId) => {
            const isActive = panelId === id;
            registeredPanel.tab.classList.toggle('active', isActive);
            registeredPanel.element.classList.toggle('active', isActive);
        });

        if (!focusOnly) {
            panel.onShow?.();
        }

        this.persistState();
    }

    ensureExpanded() {
        if (this.isCollapsed) {
            this.container.classList.remove('collapsed');
            this.isCollapsed = false;
        }
        this.applyHeight(this.height);
    }

    collapse() {
        if (this.isCollapsed) return;
        this.container.classList.add('collapsed');
        this.isCollapsed = true;

        if (this.activePanel) {
            const panel = this.panels.get(this.activePanel);
            panel?.onHide?.();
        }

        this.persistState();
    }

    expand() {
        const desired = Math.floor(window.innerHeight * 0.65);
        this.setHeight(desired);
        this.maximized = true;
        this.ensureExpanded();
    }

    restoreSize() {
        this.maximized = false;
        this.setHeight(this.defaultHeight);
        this.ensureExpanded();
    }

    hidePanelContent(id) {
        const panel = this.panels.get(id);
        if (!panel) return;

        panel.tab.classList.remove('active');
        panel.element.classList.remove('active');
        panel.onHide?.();
    }

    registerResizeObserver() {
        const observer = new ResizeObserver(() => {
            if (!this.isCollapsed) {
                this.persistState();
            }
        });

        observer.observe(this.container);
    }

    createControls() {
        const controls = document.createElement('div');
        controls.id = 'bottom-panel-controls';

        const expandBtn = document.createElement('button');
        expandBtn.type = 'button';
        expandBtn.className = 'bottom-panel-btn';
        expandBtn.textContent = '⬆ Expand';
        expandBtn.title = 'Expand bottom panel';
        expandBtn.addEventListener('click', () => {
            if (this.maximized) {
                this.restoreSize();
                expandBtn.textContent = '⬆ Expand';
                expandBtn.title = 'Expand bottom panel';
            } else {
                this.expand();
                expandBtn.textContent = '⬇ Restore';
                expandBtn.title = 'Restore bottom panel size';
            }
        });

        const collapseBtn = document.createElement('button');
        collapseBtn.type = 'button';
        collapseBtn.className = 'bottom-panel-btn';
        collapseBtn.textContent = '⛶ Hide';
        collapseBtn.title = 'Collapse bottom panel';
        collapseBtn.addEventListener('click', () => this.collapse());

        controls.appendChild(expandBtn);
        controls.appendChild(collapseBtn);
        this.tabsContainer.appendChild(controls);

        this.controls = { expandBtn, collapseBtn };
        if (this.maximized) {
            expandBtn.textContent = '⬇ Restore';
            expandBtn.title = 'Restore bottom panel size';
        }
    }

    applyHeight(height) {
        const min = this.minHeight;
        const max = Math.floor(window.innerHeight * 0.85);
        const normalized = Math.min(Math.max(height || this.defaultHeight, min), max);
        this.height = normalized;
        this.container.style.setProperty('--bottom-panel-height', `${normalized}px`);
    }

    setHeight(height) {
        this.applyHeight(height);
        this.persistState();
    }

    loadHeight() {
        try {
            const raw = localStorage.getItem('bottom-panel-height');
            const value = parseInt(raw, 10);
            if (Number.isFinite(value)) {
                return value;
            }
        } catch (error) {
            console.warn('[BottomPanel] ⚠️ Unable to load height:', error);
        }
        return this.defaultHeight;
    }

    persistState() {
        try {
            const state = {
                activePanel: this.activePanel,
                collapsed: this.isCollapsed,
                height: this.height,
                maximized: this.maximized,
            };
            localStorage.setItem('bottom-panel-state', JSON.stringify(state));
            localStorage.setItem('bottom-panel-height', String(this.height));
        } catch (error) {
            console.warn('[BottomPanel] ⚠️ Unable to persist state:', error);
        }
    }

    restoreState() {
        try {
            const raw = localStorage.getItem('bottom-panel-state');
            if (!raw) return;

            const state = JSON.parse(raw);
            this.isCollapsed = Boolean(state.collapsed);
            if (this.isCollapsed) {
                this.container.classList.add('collapsed');
            } else {
                this.container.classList.remove('collapsed');
            }
            this.activePanel = state.activePanel || null;
            if (state.height) {
                this.height = state.height;
            }
            this.maximized = Boolean(state.maximized);
        } catch (error) {
            console.warn('[BottomPanel] ⚠️ Unable to restore state:', error);
        }
    }
}

window.bottomDock = new BottomPanelManager();

console.log('[BottomPanel] 🎛️ Bottom panel manager loaded');

})();

