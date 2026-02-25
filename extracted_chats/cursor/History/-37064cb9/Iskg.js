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

        this.restoreState();
        this.registerResizeObserver();
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

    persistState() {
        try {
            const state = {
                activePanel: this.activePanel,
                collapsed: this.isCollapsed,
            };
            localStorage.setItem('bottom-panel-state', JSON.stringify(state));
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
        } catch (error) {
            console.warn('[BottomPanel] ⚠️ Unable to restore state:', error);
        }
    }
}

window.bottomDock = new BottomPanelManager();

console.log('[BottomPanel] 🎛️ Bottom panel manager loaded');

})();

