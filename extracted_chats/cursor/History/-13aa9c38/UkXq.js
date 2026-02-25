(function() {
'use strict';

if (!window.electron || !window.electron.settings) {
    console.warn('[SettingsApplier] ⚠️ Settings API unavailable');
    return;
}

const settingsApi = window.electron.settings;
const state = {
    current: null,
    defaults: null
};

const COLOR_MAP = {
    '--cursor-bg': { key: 'backgroundPrimary', opacityKey: 'window' },
    '--cursor-bg-secondary': { key: 'backgroundSecondary', opacityKey: 'sidePanels' },
    '--cursor-bg-hover': { key: 'accentSoft', opacityKey: 'sidePanels' },
    '--cursor-input-bg': { key: 'backgroundFloating', opacityKey: 'chatPanels' },
    '--cursor-border': { key: 'border' },
    '--cursor-text': { key: 'textPrimary' },
    '--cursor-text-secondary': { key: 'textSecondary' },
    '--cursor-text-muted': { key: 'textSecondary' },
    '--cursor-accent': { key: 'accent' },
    '--cursor-accent-hover': { key: 'accent' },
    '--cursor-jade-light': { key: 'accentSoft' },
    '--cursor-jade-medium': { key: 'accent' },
    '--cursor-jade-dark': { key: 'accent' },
    '--cursor-shadow': { key: 'accentSoft' }
};

function clone(value) {
    if (Array.isArray(value)) return value.map(clone);
    if (value && typeof value === 'object') {
        return Object.keys(value).reduce((acc, key) => {
            acc[key] = clone(value[key]);
            return acc;
        }, {});
    }
    return value;
}

function mergeDeep(target, source) {
    if (!source || typeof source !== 'object') {
        return target;
    }
    const output = Array.isArray(target) ? target.slice() : { ...target };
    for (const [key, value] of Object.entries(source)) {
        if (value && typeof value === 'object' && !Array.isArray(value)) {
            output[key] = mergeDeep(output[key] || {}, value);
        } else {
            output[key] = clone(value);
        }
    }
    return output;
}

function setPath(target, pathString, value) {
    if (!target) return;
    const segments = pathString.split('.');
    let current = target;
    for (let i = 0; i < segments.length - 1; i++) {
        const segment = segments[i];
        if (!current[segment] || typeof current[segment] !== 'object') {
            current[segment] = {};
        }
        current = current[segment];
    }
    current[segments[segments.length - 1]] = clone(value);
}

function getPath(target, pathString) {
    const segments = pathString.split('.');
    let current = target;
    for (const segment of segments) {
        if (current == null) return undefined;
        current = current[segment];
    }
    return current;
}

function withAlpha(color, alpha) {
    if (alpha == null) return color;
    if (alpha >= 1) return color;
    if (!color) return color;

    if (/^#/.test(color)) {
        const hex = color.replace('#', '');
        if (hex.length === 3) {
            const r = parseInt(hex[0] + hex[0], 16);
            const g = parseInt(hex[1] + hex[1], 16);
            const b = parseInt(hex[2] + hex[2], 16);
            return `rgba(${r}, ${g}, ${b}, ${alpha})`;
        }
        if (hex.length === 6) {
            const r = parseInt(hex.substring(0, 2), 16);
            const g = parseInt(hex.substring(2, 4), 16);
            const b = parseInt(hex.substring(4, 6), 16);
            return `rgba(${r}, ${g}, ${b}, ${alpha})`;
        }
        return color;
    }

    if (/rgba?\(/i.test(color)) {
        return color.replace(/rgba?\(([^)]+)\)/, (full, values) => {
            const parts = values.split(',').map(part => part.trim());
            const [r, g, b] = parts;
            return `rgba(${r}, ${g}, ${b}, ${alpha})`;
        });
    }

    return color;
}

function applyAppearance(appearance) {
    if (!appearance) return;

    const root = document.documentElement;
    root.style.setProperty('--app-font-family', appearance.fontFamily || "'Segoe UI', sans-serif");
    const fontSize = appearance.fontSize ? `${appearance.fontSize}px` : '14px';
    root.style.setProperty('--app-font-size', fontSize);
    const lineHeight = appearance.lineHeight ? `${appearance.lineHeight}` : '1.6';
    root.style.setProperty('--app-line-height', lineHeight);
    const uiScale = appearance.uiScale || 1;
    root.style.setProperty('--app-ui-scale', uiScale);

    const colors = appearance.colors || {};
    const transparency = appearance.transparency || {};
    const transparencyEnabled = Boolean(transparency.enabled);

    Object.entries(COLOR_MAP).forEach(([cssVar, map]) => {
        let value = colors[map.key];
        if (transparencyEnabled && map.opacityKey && transparency[map.opacityKey] != null) {
            value = withAlpha(value, transparency[map.opacityKey]);
        }
        if (value) {
            root.style.setProperty(cssVar, value);
        }
    });

    // Editor-specific font
    if (appearance.monospaceFont) {
        root.style.setProperty('--app-monospace-font', appearance.monospaceFont);
    }

    document.body.style.backgroundColor = colors.backgroundPrimary || getComputedStyle(root).getPropertyValue('--cursor-bg');
}

function applyLayout(layout) {
    if (!layout) return;
    document.body.classList.toggle('layout-no-overlap', layout.allowOverlap === false);
}

function applySnapshot(settings) {
    if (!settings) return;
    state.current = clone(settings);
    applyAppearance(settings.appearance);
    applyLayout(settings.layout);
}

function handleUpdateEvent(event) {
    if (!event) return;
    if (!state.current) {
        // Fallback to full reload if snapshot missing
        settingsApi.getAll().then((res) => {
            if (res?.success) {
                applySnapshot(res.settings);
            }
        });
        return;
    }

    switch (event.type) {
        case 'set':
            if (event.path) {
                setPath(state.current, event.path, event.value);
            }
            break;
        case 'update':
            if (event.changes) {
                state.current = mergeDeep(state.current || {}, event.changes);
            }
            break;
        case 'reset':
            if (event.section && state.defaults) {
                const defaultsSection = getPath(state.defaults, event.section);
                if (defaultsSection !== undefined) {
                    setPath(state.current, event.section, clone(defaultsSection));
                }
            } else if (state.defaults) {
                state.current = clone(state.defaults);
            }
            break;
        case 'hotkey':
            if (event.action) {
                setPath(state.current, `hotkeys.${event.action}`, {
                    ...(getPath(state.current, `hotkeys.${event.action}`) || {}),
                    combo: event.combo
                });
            }
            break;
        default:
            break;
    }

    applyAppearance(state.current.appearance);
    applyLayout(state.current.layout);
}

settingsApi.getDefaults().then((res) => {
    if (res?.success) {
        state.defaults = clone(res.settings);
    }
});

settingsApi.getAll().then((res) => {
    if (res?.success) {
        applySnapshot(res.settings);
    }
});

settingsApi.onBootstrap((settings) => {
    applySnapshot(settings);
});

settingsApi.onDidChange((event) => {
    handleUpdateEvent(event);
});

})();
