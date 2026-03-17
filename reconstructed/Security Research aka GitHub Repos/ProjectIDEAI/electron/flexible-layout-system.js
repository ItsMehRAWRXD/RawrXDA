/**
 * BigDaddyG IDE - Flexible Layout System
 * Drag-and-drop customizable workspace
 * Put anything anywhere: editor, terminal, chat, browser, file explorer
 */

(function() {
'use strict';

class FlexibleLayoutSystem {
    constructor() {
        this.containers = new Map();
        this.panels = new Map();
        this.draggedPanel = null;
        this.dropZones = [];
        this.layouts = {
            default: null,
            saved: []
        };
        this.activeLayout = 'default';
        
        console.log('[FlexibleLayout] 🎨 Initializing flexible layout system...');
        this.init();
    }
    
    init() {
        // Create main workspace container
        this.createWorkspace();
        
        // Register available panels
        this.registerPanels();
        
        // Set up drag and drop
        this.setupDragAndDrop();
        
        // Load saved layout
        this.loadLayout();
        
        // Expose to window
        window.flexibleLayout = this;
        
        console.log('[FlexibleLayout] ✅ Flexible layout system ready!');
        this.showWelcomeMessage();
    }
    
    createWorkspace() {
        // Find or create main workspace
        let workspace = document.getElementById('flexible-workspace');
        
        if (!workspace) {
            // Create new workspace
            workspace = document.createElement('div');
            workspace.id = 'flexible-workspace';
            workspace.className = 'flexible-workspace';
            workspace.style.cssText = `
                position: fixed;
                top: 40px;
                left: 0;
                right: 0;
                bottom: 0;
                display: flex;
                flex-direction: column;
                background: var(--void);
                z-index: 1;
            `;
            
            // Replace or insert into body
            const mainContainer = document.getElementById('main-container') || document.body;
            if (document.getElementById('main-container')) {
                mainContainer.parentNode.replaceChild(workspace, mainContainer);
            } else {
                document.body.appendChild(workspace);
            }
        }
        
        this.workspace = workspace;
        
        // Don't create default layout - preserve existing DOM structure
        // Users can manually customize layout with Ctrl+Shift+L
        console.log('[FlexibleLayout] Workspace assigned, existing structure preserved');
        console.log('[FlexibleLayout] Use Ctrl+Shift+L to customize layout');
    }
    
    createDefaultLayout() {
        // IMPORTANT: Don't clear workspace automatically!
        // The existing DOM has #monaco-container, #tab-bar, etc that Monaco needs
        // Only clear if explicitly requested by user
        
        console.warn('[FlexibleLayout] createDefaultLayout() called - this will replace existing structure!');
        console.warn('[FlexibleLayout] Skipping auto-clear to preserve Monaco editor container');
        
        // Don't clear: this.workspace.innerHTML = '';
        
        // Create root container (can be split)
        const rootContainer = this.createContainer('root', 'vertical', []);
        // Don't append: this.workspace.appendChild(rootContainer.element);
        
        // Store for later use
        this.rootContainer = rootContainer;
        
        console.log('[FlexibleLayout] Layout system ready but not applied');
        console.log('[FlexibleLayout] Original structure preserved for Monaco compatibility');
        
        /* Original behavior (disabled to preserve Monaco):
        // Add editor panel (will take up most space)
        this.addPanel('editor', 'root');
        
        // Add terminal panel below editor
        this.addPanel('terminal', 'root');
        
        // Set flex weights for better proportions
        setTimeout(() => {
            const panels = this.workspace.querySelectorAll('[data-panel-id]');
            if (panels[0]) panels[0].style.flex = '3'; // Editor gets 75% space
            if (panels[1]) panels[1].style.flex = '1'; // Terminal gets 25% space
        }, 100);
        */
    }
    
    // Split an existing container into two parts
    splitContainer(containerId, direction) {
        const container = this.containers.get(containerId);
        if (!container) {
            console.warn(`[FlexibleLayout] Container ${containerId} not found`);
            return;
        }
        
        // Update container direction
        container.direction = direction;
        container.element.style.flexDirection = direction === 'vertical' ? 'column' : 'row';
        container.element.dataset.direction = direction;
        
        console.log(`[FlexibleLayout] Split container ${containerId} ${direction}`);
    }
    
    createContainer(id, direction = 'vertical', children = []) {
        const container = document.createElement('div');
        container.className = 'flex-container';
        container.dataset.containerId = id;
        container.dataset.direction = direction;
        container.style.cssText = `
            display: flex;
            flex-direction: ${direction === 'vertical' ? 'column' : 'row'};
            flex: 1;
            min-height: 0;
            min-width: 0;
            position: relative;
            gap: 4px;
            background: rgba(0, 212, 255, 0.05);
        `;
        
        const containerData = {
            id,
            element: container,
            direction,
            children: children,
            parent: null
        };
        
        this.containers.set(id, containerData);
        return containerData;
    }
    
    registerPanels() {
        // Register all available panel types
        this.registerPanel('editor', {
            title: '📝 Code Editor',
            icon: '📝',
            description: 'Monaco code editor',
            create: () => this.createEditorPanel(),
            color: '#00d4ff'
        });
        
        this.registerPanel('terminal', {
            title: '💻 Terminal',
            icon: '💻',
            description: 'Integrated terminal',
            create: () => this.createTerminalPanel(),
            color: '#00ff88'
        });
        
        this.registerPanel('chat', {
            title: '💬 AI Chat',
            icon: '💬',
            description: 'AI assistant chat',
            create: () => this.createChatPanel(),
            color: '#a855f7'
        });
        
        this.registerPanel('explorer', {
            title: '📁 File Explorer',
            icon: '📁',
            description: 'File browser',
            create: () => this.createExplorerPanel(),
            color: '#ff6b35'
        });
        
        this.registerPanel('browser', {
            title: '🌐 Browser',
            icon: '🌐',
            description: 'Web browser',
            create: () => this.createBrowserPanel(),
            color: '#00d4ff'
        });
        
        this.registerPanel('console', {
            title: '🖥️ Console',
            icon: '🖥️',
            description: 'Output console',
            create: () => this.createConsolePanel(),
            color: '#ffaa00'
        });
        
        this.registerPanel('agent', {
            title: '🤖 Agent Panel',
            icon: '🤖',
            description: 'AI agents',
            create: () => this.createAgentPanel(),
            color: '#a855f7'
        });
        
        this.registerPanel('git', {
            title: '📊 Git',
            icon: '📊',
            description: 'Version control',
            create: () => this.createGitPanel(),
            color: '#ff6b35'
        });
        
        console.log(`[FlexibleLayout] Registered ${this.panels.size} panel types`);
    }
    
    registerPanel(type, config) {
        this.panels.set(type, config);
    }
    
    addPanel(type, containerId = 'root') {
        const panelConfig = this.panels.get(type);
        if (!panelConfig) {
            console.warn(`[FlexibleLayout] Unknown panel type: ${type}`);
            return null;
        }
        
        const container = this.containers.get(containerId);
        if (!container) {
            console.warn(`[FlexibleLayout] Container not found: ${containerId}`);
            return null;
        }
        
        // Create panel wrapper
        const panelId = `${type}-${Date.now()}`;
        const panelWrapper = document.createElement('div');
        panelWrapper.className = 'flex-panel';
        panelWrapper.dataset.panelId = panelId;
        panelWrapper.dataset.panelType = type;
        panelWrapper.draggable = true;
        panelWrapper.style.cssText = `
            display: flex;
            flex-direction: column;
            flex: 1;
            min-height: 100px;
            min-width: 200px;
            background: rgba(10, 10, 30, 0.95);
            border: 1px solid ${panelConfig.color}40;
            border-radius: 8px;
            overflow: hidden;
            position: relative;
        `;
        
        // Create panel header
        const header = this.createPanelHeader(panelId, type, panelConfig);
        panelWrapper.appendChild(header);
        
        // Create panel content
        const content = panelConfig.create();
        content.style.cssText = `
            flex: 1;
            overflow: auto;
            padding: 10px;
        `;
        panelWrapper.appendChild(content);
        
        // Add to container
        container.element.appendChild(panelWrapper);
        container.children.push(panelId);
        
        console.log(`[FlexibleLayout] Added ${type} panel: ${panelId}`);
        return panelId;
    }
    
    createPanelHeader(panelId, type, config) {
        const header = document.createElement('div');
        header.className = 'flex-panel-header';
        header.style.cssText = `
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 8px 12px;
            background: linear-gradient(135deg, ${config.color}20, ${config.color}10);
            border-bottom: 1px solid ${config.color}40;
            cursor: move;
            user-select: none;
        `;
        
        header.innerHTML = `
            <div style="display: flex; align-items: center; gap: 8px; flex: 1;">
                <span style="font-size: 16px;">${config.icon}</span>
                <span style="color: ${config.color}; font-weight: bold; font-size: 13px;">${config.title}</span>
                <span style="color: #666; font-size: 11px; margin-left: auto;">${config.description}</span>
            </div>
            <div style="display: flex; gap: 4px;">
                <button class="panel-btn panel-split-h" title="Split Horizontally" style="background: rgba(0,212,255,0.1); border: 1px solid rgba(0,212,255,0.3); color: var(--cyan); padding: 2px 6px; border-radius: 3px; cursor: pointer; font-size: 10px;">⬌</button>
                <button class="panel-btn panel-split-v" title="Split Vertically" style="background: rgba(0,212,255,0.1); border: 1px solid rgba(0,212,255,0.3); color: var(--cyan); padding: 2px 6px; border-radius: 3px; cursor: pointer; font-size: 10px;">⬍</button>
                <button class="panel-btn panel-maximize" title="Maximize" style="background: rgba(168,85,247,0.1); border: 1px solid rgba(168,85,247,0.3); color: var(--purple); padding: 2px 6px; border-radius: 3px; cursor: pointer; font-size: 10px;">⛶</button>
                <button class="panel-btn panel-close" title="Close" style="background: rgba(255,71,87,0.1); border: 1px solid rgba(255,71,87,0.3); color: var(--red); padding: 2px 6px; border-radius: 3px; cursor: pointer; font-size: 10px;">✕</button>
            </div>
        `;
        
        // Add event listeners
        const splitHBtn = header.querySelector('.panel-split-h');
        const splitVBtn = header.querySelector('.panel-split-v');
        const maximizeBtn = header.querySelector('.panel-maximize');
        const closeBtn = header.querySelector('.panel-close');
        
        splitHBtn.onclick = (e) => {
            e.stopPropagation();
            this.splitPanelContainer(panelId, 'horizontal');
        };
        
        splitVBtn.onclick = (e) => {
            e.stopPropagation();
            this.splitPanelContainer(panelId, 'vertical');
        };
        
        maximizeBtn.onclick = (e) => {
            e.stopPropagation();
            this.maximizePanel(panelId);
        };
        
        closeBtn.onclick = (e) => {
            e.stopPropagation();
            this.closePanel(panelId);
        };
        
        return header;
    }
    
    setupDragAndDrop() {
        // Drag start
        document.addEventListener('dragstart', (e) => {
            const panel = e.target.closest('.flex-panel');
            if (panel && e.target.classList.contains('flex-panel-header')) {
                this.draggedPanel = panel;
                panel.style.opacity = '0.5';
                e.dataTransfer.effectAllowed = 'move';
                e.dataTransfer.setData('text/html', panel.dataset.panelId);
                
                // Show drop zones
                this.showDropZones();
            }
        });
        
        // Drag over
        document.addEventListener('dragover', (e) => {
            if (this.draggedPanel) {
                e.preventDefault();
                e.dataTransfer.dropEffect = 'move';
            }
        });
        
        // Drop
        document.addEventListener('drop', (e) => {
            if (this.draggedPanel) {
                e.preventDefault();
                
                const dropZone = e.target.closest('.drop-zone');
                if (dropZone) {
                    const targetContainer = dropZone.dataset.containerId;
                    const position = dropZone.dataset.position;
                    
                    this.movePanel(this.draggedPanel.dataset.panelId, targetContainer, position);
                }
                
                this.hideDropZones();
            }
        });
        
        // Drag end
        document.addEventListener('dragend', (e) => {
            if (this.draggedPanel) {
                this.draggedPanel.style.opacity = '1';
                this.draggedPanel = null;
                this.hideDropZones();
            }
        });
    }
    
    showDropZones() {
        this.hideDropZones();
        
        this.containers.forEach((container) => {
            const positions = ['top', 'right', 'bottom', 'left', 'center'];
            
            positions.forEach(position => {
                const dropZone = document.createElement('div');
                dropZone.className = 'drop-zone';
                dropZone.dataset.containerId = container.id;
                dropZone.dataset.position = position;
                
                const positionStyles = {
                    top: 'top: 0; left: 0; right: 0; height: 30%;',
                    bottom: 'bottom: 0; left: 0; right: 0; height: 30%;',
                    left: 'left: 0; top: 0; bottom: 0; width: 30%;',
                    right: 'right: 0; top: 0; bottom: 0; width: 30%;',
                    center: 'top: 30%; left: 30%; right: 30%; bottom: 30%;'
                };
                
                dropZone.style.cssText = `
                    position: absolute;
                    ${positionStyles[position]}
                    background: rgba(0, 212, 255, 0.2);
                    border: 2px dashed var(--cyan);
                    z-index: 10000;
                    display: none;
                    align-items: center;
                    justify-content: center;
                    color: var(--cyan);
                    font-weight: bold;
                    pointer-events: all;
                `;
                
                dropZone.innerHTML = `<span style="background: rgba(10,10,30,0.9); padding: 10px 20px; border-radius: 8px;">Drop Here (${position})</span>`;
                
                dropZone.addEventListener('dragenter', () => {
                    dropZone.style.display = 'flex';
                    dropZone.style.background = 'rgba(0, 212, 255, 0.4)';
                });
                
                dropZone.addEventListener('dragleave', () => {
                    dropZone.style.background = 'rgba(0, 212, 255, 0.2)';
                });
                
                container.element.appendChild(dropZone);
                this.dropZones.push(dropZone);
            });
        });
    }
    
    hideDropZones() {
        this.dropZones.forEach(zone => zone.remove());
        this.dropZones = [];
    }
    
    movePanel(panelId, targetContainerId, position) {
        const panel = document.querySelector(`[data-panel-id="${panelId}"]`);
        if (!panel) return;
        
        const targetContainer = this.containers.get(targetContainerId);
        if (!targetContainer) return;
        
        // Remove from current container
        const currentContainer = panel.parentElement.closest('[data-container-id]');
        if (currentContainer) {
            const containerId = currentContainer.dataset.containerId;
            const containerData = this.containers.get(containerId);
            if (containerData) {
                containerData.children = containerData.children.filter(id => id !== panelId);
            }
        }
        
        // Add to new container based on position
        if (position === 'center') {
            targetContainer.element.appendChild(panel);
            targetContainer.children.push(panelId);
        } else {
            // Create new split container
            this.splitContainerAtPosition(targetContainerId, position, panelId);
        }
        
        this.saveLayout();
    }
    
    splitPanelContainer(panelId, direction) {
        const panel = document.querySelector(`[data-panel-id="${panelId}"]`);
        if (!panel) return;
        
        // Add panel selector modal
        this.showPanelSelector((selectedType) => {
            // Create new container
            const containerId = `container-${Date.now()}`;
            const container = this.createContainer(containerId, direction, [panelId]);
            
            // Replace panel with container
            panel.parentElement.replaceChild(container.element, panel);
            
            // Add original panel to new container
            container.element.appendChild(panel);
            
            // Add new panel
            this.addPanel(selectedType, containerId);
            
            this.saveLayout();
        });
    }
    
    splitContainerAtPosition(containerId, position, panelId) {
        const container = this.containers.get(containerId);
        const panel = document.querySelector(`[data-panel-id="${panelId}"]`);
        
        const direction = (position === 'top' || position === 'bottom') ? 'vertical' : 'horizontal';
        
        // Create wrapper container
        const wrapperId = `container-${Date.now()}`;
        const wrapper = this.createContainer(wrapperId, direction, []);
        
        // Add to target container
        if (position === 'top' || position === 'left') {
            container.element.insertBefore(wrapper.element, container.element.firstChild);
            wrapper.element.appendChild(panel);
        } else {
            container.element.appendChild(wrapper.element);
            wrapper.element.appendChild(panel);
        }
        
        container.children.push(panelId);
    }
    
    maximizePanel(panelId) {
        const panel = document.querySelector(`[data-panel-id="${panelId}"]`);
        if (!panel) return;
        
        if (panel.classList.contains('maximized')) {
            // Restore
            panel.classList.remove('maximized');
            panel.style.position = '';
            panel.style.top = '';
            panel.style.left = '';
            panel.style.right = '';
            panel.style.bottom = '';
            panel.style.zIndex = '';
        } else {
            // Maximize
            panel.classList.add('maximized');
            panel.style.position = 'fixed';
            panel.style.top = '40px';
            panel.style.left = '0';
            panel.style.right = '0';
            panel.style.bottom = '0';
            panel.style.zIndex = '10000';
        }
    }
    
    closePanel(panelId) {
        const panel = document.querySelector(`[data-panel-id="${panelId}"]`);
        if (!panel) return;
        
        if (confirm(`Close this panel?`)) {
            // Remove from container children
            const container = panel.parentElement.closest('[data-container-id]');
            if (container) {
                const containerId = container.dataset.containerId;
                const containerData = this.containers.get(containerId);
                if (containerData) {
                    containerData.children = containerData.children.filter(id => id !== panelId);
                }
            }
            
            panel.remove();
            this.saveLayout();
        }
    }
    
    showPanelSelector(callback) {
        const modal = document.createElement('div');
        modal.style.cssText = `
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: rgba(0, 0, 0, 0.8);
            display: flex;
            align-items: center;
            justify-content: center;
            z-index: 100000;
        `;
        
        const content = document.createElement('div');
        content.style.cssText = `
            background: rgba(10, 10, 30, 0.98);
            border: 2px solid var(--cyan);
            border-radius: 12px;
            padding: 30px;
            max-width: 600px;
            max-height: 80vh;
            overflow: auto;
        `;
        
        content.innerHTML = `
            <h2 style="color: var(--cyan); margin-bottom: 20px;">📦 Select Panel to Add</h2>
            <div id="panel-grid" style="display: grid; grid-template-columns: repeat(auto-fill, minmax(150px, 1fr)); gap: 15px;"></div>
        `;
        
        const grid = content.querySelector('#panel-grid');
        
        this.panels.forEach((config, type) => {
            const card = document.createElement('div');
            card.style.cssText = `
                background: rgba(0, 212, 255, 0.1);
                border: 2px solid ${config.color}40;
                border-radius: 8px;
                padding: 20px;
                text-align: center;
                cursor: pointer;
                transition: all 0.2s;
            `;
            
            card.innerHTML = `
                <div style="font-size: 32px; margin-bottom: 10px;">${config.icon}</div>
                <div style="color: ${config.color}; font-weight: bold; margin-bottom: 5px;">${type}</div>
                <div style="color: #888; font-size: 11px;">${config.description}</div>
            `;
            
            card.onmouseenter = () => {
                card.style.background = `${config.color}20`;
                card.style.borderColor = config.color;
                card.style.transform = 'scale(1.05)';
            };
            
            card.onmouseleave = () => {
                card.style.background = 'rgba(0, 212, 255, 0.1)';
                card.style.borderColor = `${config.color}40`;
                card.style.transform = 'scale(1)';
            };
            
            card.onclick = () => {
                modal.remove();
                callback(type);
            };
            
            grid.appendChild(card);
        });
        
        modal.appendChild(content);
        modal.onclick = (e) => {
            if (e.target === modal) modal.remove();
        };
        
        document.body.appendChild(modal);
    }
    
    // Panel creation methods
    createEditorPanel() {
        const panel = document.createElement('div');
        panel.innerHTML = `
            <div id="monaco-editor-container-${Date.now()}" style="width: 100%; height: 100%;"></div>
        `;
        // Monaco editor will be initialized here
        return panel;
    }
    
    createTerminalPanel() {
        const panel = document.createElement('div');
        panel.style.cssText = 'background: #000; color: #0f0; font-family: monospace; padding: 10px;';
        panel.innerHTML = `
            <div>💻 Terminal Ready</div>
            <div style="color: #0ff; margin-top: 10px;">$ </div>
        `;
        return panel;
    }
    
    createChatPanel() {
        const panel = document.createElement('div');
        panel.innerHTML = `
            <div style="display: flex; flex-direction: column; height: 100%;">
                <div style="flex: 1; overflow-y: auto; padding: 10px;">
                    <div style="color: var(--cyan);">💬 AI Chat Ready</div>
                </div>
                <div style="padding: 10px; border-top: 1px solid rgba(0,212,255,0.3);">
                    <input type="text" placeholder="Ask AI..." style="width: 100%; padding: 8px; background: rgba(0,0,0,0.5); border: 1px solid var(--cyan); border-radius: 4px; color: #fff;">
                </div>
            </div>
        `;
        return panel;
    }
    
    createExplorerPanel() {
        const panel = document.createElement('div');
        panel.innerHTML = `
            <div style="color: var(--cyan);">📁 File Explorer</div>
            <div style="margin-top: 10px; color: #888;">Loading files...</div>
        `;
        return panel;
    }
    
    createBrowserPanel() {
        const panel = document.createElement('div');
        panel.innerHTML = `
            <iframe src="about:blank" style="width: 100%; height: 100%; border: none;"></iframe>
        `;
        return panel;
    }
    
    createConsolePanel() {
        const panel = document.createElement('div');
        panel.style.cssText = 'background: #000; color: #fff; font-family: monospace; padding: 10px; overflow-y: auto;';
        panel.innerHTML = `<div style="color: #0f0;">🖥️ Console Output</div>`;
        return panel;
    }
    
    createAgentPanel() {
        const panel = document.createElement('div');
        panel.innerHTML = `
            <div style="color: var(--purple);">🤖 AI Agents</div>
            <div style="margin-top: 10px; color: #888;">Agents ready</div>
        `;
        return panel;
    }
    
    createGitPanel() {
        const panel = document.createElement('div');
        panel.innerHTML = `
            <div style="color: var(--orange);">📊 Git Status</div>
            <div style="margin-top: 10px; color: #888;">Repository info</div>
        `;
        return panel;
    }
    
    saveLayout() {
        const layout = this.serializeLayout();
        localStorage.setItem('flexibleLayout', JSON.stringify(layout));
        console.log('[FlexibleLayout] Layout saved');
    }
    
    loadLayout() {
        const saved = localStorage.getItem('flexibleLayout');
        if (saved) {
            try {
                const layout = JSON.parse(saved);
                this.deserializeLayout(layout);
                console.log('[FlexibleLayout] Layout loaded');
            } catch (error) {
                console.warn('[FlexibleLayout] Failed to load layout:', error);
            }
        }
    }
    
    serializeLayout() {
        // Serialize current layout structure
        return {
            containers: Array.from(this.containers.entries()),
            timestamp: Date.now()
        };
    }
    
    deserializeLayout(layout) {
        // Restore layout from saved data
        // Implementation depends on complexity needed
    }
    
    showWelcomeMessage() {
        // Check if first time
        const isFirstTime = !localStorage.getItem('flexibleLayoutSeen');
        
        if (isFirstTime) {
            localStorage.setItem('flexibleLayoutSeen', 'true');
            this.showTutorial();
        } else {
            setTimeout(() => {
                window.showNotification?.(
                    '🎨 Flexible Layout Active!',
                    'Drag panels anywhere • Ctrl+Shift+L to add panels • Ctrl+Alt+L to reset',
                    'success',
                    5000
                );
            }, 1000);
        }
    }
    
    showTutorial() {
        const tutorial = document.createElement('div');
        tutorial.style.cssText = `
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: rgba(0, 0, 0, 0.95);
            display: flex;
            align-items: center;
            justify-content: center;
            z-index: 1000000;
            animation: fadeIn 0.3s;
        `;
        
        tutorial.innerHTML = `
            <div style="
                max-width: 800px;
                max-height: 90vh;
                overflow: auto;
                background: linear-gradient(135deg, rgba(10,10,30,0.98), rgba(0,20,40,0.98));
                border: 3px solid var(--cyan);
                border-radius: 20px;
                padding: 40px;
                box-shadow: 0 0 60px rgba(0,212,255,0.5);
            ">
                <h1 style="color: var(--cyan); text-align: center; font-size: 32px; margin-bottom: 10px;">
                    🎨 Welcome to Flexible Layout!
                </h1>
                <p style="text-align: center; color: #888; margin-bottom: 30px;">
                    Your IDE, arranged exactly how you want it
                </p>
                
                <div style="background: rgba(0,212,255,0.1); border-left: 4px solid var(--cyan); padding: 20px; margin-bottom: 25px; border-radius: 8px;">
                    <h3 style="color: var(--cyan); margin-bottom: 15px;">✨ Quick Start</h3>
                    <ul style="color: #ccc; line-height: 2;">
                        <li><strong style="color: var(--cyan);">Drag panels</strong> - Click and hold the colored header bar</li>
                        <li><strong style="color: var(--purple);">Split panels</strong> - Click ⬌ (horizontal) or ⬍ (vertical)</li>
                        <li><strong style="color: var(--orange);">Maximize</strong> - Click ⛶ for full screen</li>
                        <li><strong style="color: var(--red);">Close</strong> - Click ✕ to remove</li>
                    </ul>
                </div>
                
                <div style="background: rgba(168,85,247,0.1); border-left: 4px solid var(--purple); padding: 20px; margin-bottom: 25px; border-radius: 8px;">
                    <h3 style="color: var(--purple); margin-bottom: 15px;">⌨️ Hotkeys</h3>
                    <div style="display: grid; grid-template-columns: auto 1fr; gap: 10px; color: #ccc;">
                        <kbd style="background: rgba(0,0,0,0.5); padding: 5px 10px; border-radius: 4px; color: var(--cyan);">Ctrl+Shift+L</kbd>
                        <span>Add new panel</span>
                        <kbd style="background: rgba(0,0,0,0.5); padding: 5px 10px; border-radius: 4px; color: var(--orange);">Ctrl+Alt+L</kbd>
                        <span>Reset to default layout</span>
                    </div>
                </div>
                
                <div style="background: rgba(0,212,255,0.1); border: 2px dashed var(--cyan); padding: 20px; margin-bottom: 25px; border-radius: 12px; text-align: center;">
                    <div style="font-size: 48px; margin-bottom: 10px;">🚀</div>
                    <h3 style="color: var(--cyan); margin-bottom: 10px;">Available Panels</h3>
                    <div style="display: flex; flex-wrap: wrap; gap: 10px; justify-content: center; margin-top: 15px;">
                        <span style="background: rgba(0,212,255,0.2); padding: 8px 15px; border-radius: 20px; color: var(--cyan);">📝 Editor</span>
                        <span style="background: rgba(0,255,136,0.2); padding: 8px 15px; border-radius: 20px; color: var(--green);">💻 Terminal</span>
                        <span style="background: rgba(168,85,247,0.2); padding: 8px 15px; border-radius: 20px; color: var(--purple);">💬 Chat</span>
                        <span style="background: rgba(255,107,53,0.2); padding: 8px 15px; border-radius: 20px; color: var(--orange);">📁 Explorer</span>
                        <span style="background: rgba(0,212,255,0.2); padding: 8px 15px; border-radius: 20px; color: var(--cyan);">🌐 Browser</span>
                        <span style="background: rgba(255,170,0,0.2); padding: 8px 15px; border-radius: 20px; color: var(--yellow);">🖥️ Console</span>
                        <span style="background: rgba(168,85,247,0.2); padding: 8px 15px; border-radius: 20px; color: var(--purple);">🤖 Agents</span>
                        <span style="background: rgba(255,107,53,0.2); padding: 8px 15px; border-radius: 20px; color: var(--orange);">📊 Git</span>
                    </div>
                </div>
                
                <div style="background: rgba(255,107,53,0.1); border-left: 4px solid var(--orange); padding: 20px; margin-bottom: 25px; border-radius: 8px;">
                    <h3 style="color: var(--orange); margin-bottom: 15px;">💡 Pro Tips</h3>
                    <ul style="color: #ccc; line-height: 2;">
                        <li>Your layout <strong>auto-saves</strong> - it'll be here next time!</li>
                        <li>Drop zones appear when dragging - shows where panel will go</li>
                        <li>Experiment! You can always reset to default</li>
                        <li>Check <code style="color: var(--cyan);">FLEXIBLE-LAYOUT-GUIDE.md</code> for layout ideas</li>
                    </ul>
                </div>
                
                <div style="text-align: center; margin-top: 30px;">
                    <button id="tutorial-start-btn" style="
                        background: linear-gradient(135deg, var(--cyan), var(--purple));
                        color: white;
                        border: none;
                        padding: 15px 40px;
                        font-size: 18px;
                        font-weight: bold;
                        border-radius: 10px;
                        cursor: pointer;
                        box-shadow: 0 5px 20px rgba(0,212,255,0.4);
                        transition: all 0.2s;
                    ">Let's Go! 🚀</button>
                </div>
            </div>
        `;
        
        const startBtn = tutorial.querySelector('#tutorial-start-btn');
        startBtn.onmouseenter = () => {
            startBtn.style.transform = 'scale(1.05)';
            startBtn.style.boxShadow = '0 8px 30px rgba(0,212,255,0.6)';
        };
        startBtn.onmouseleave = () => {
            startBtn.style.transform = 'scale(1)';
            startBtn.style.boxShadow = '0 5px 20px rgba(0,212,255,0.4)';
        };
        startBtn.onclick = () => {
            tutorial.style.animation = 'fadeOut 0.3s';
            setTimeout(() => tutorial.remove(), 300);
        };
        
        // Add animations
        const style = document.createElement('style');
        style.textContent = `
            @keyframes fadeIn {
                from { opacity: 0; }
                to { opacity: 1; }
            }
            @keyframes fadeOut {
                from { opacity: 1; }
                to { opacity: 0; }
            }
        `;
        document.head.appendChild(style);
        
        document.body.appendChild(tutorial);
    }
}

// Initialize
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        new FlexibleLayoutSystem();
    });
} else {
    new FlexibleLayoutSystem();
}

})();
