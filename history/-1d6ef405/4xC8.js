// circular_router.js — Node.js Beacon Circular Router
// Drop-in module for beacon-manager.js
// Implements the circular ring topology: IDE ↔ Agentic ↔ Hotpatch ↔ Security ↔ IDE
// All GUI panes have equal access through the ring.

'use strict';

class CircularBeaconRouter {
    constructor() {
        this.beacons = new Map();
        this.routes = new Map();
        this.messageLog = [];
        this.maxLogSize = 1000;
        this.setupCircularRoutes();
    }

    // Define the core circular ring
    setupCircularRoutes() {
        // Primary ring: IDE → Agentic → Hotpatch → Security → IDE
        const ring = ['IDE', 'Agentic', 'Hotpatch', 'Security'];
        for (let i = 0; i < ring.length; i++) {
            const from = ring[i];
            const to = ring[(i + 1) % ring.length];
            this.registerRoute(from, to, (msg) => this.forwardToTarget(msg));
        }

        // GUI pane shortcuts — panes can reach any core subsystem directly
        const paneTypes = ['GUI_Pane', 'PowerShell', 'Todo', 'Observability',
                           'Training', 'ModelRouter', 'Chat'];
        const coreTypes = ['Agentic', 'Hotpatch', 'Security', 'IDE'];

        for (const pane of paneTypes) {
            for (const core of coreTypes) {
                this.registerRoute(pane, core, (msg) => this.forwardToTarget(msg));
            }
        }

        // Bidirectional: core systems can push to panes
        for (const core of coreTypes) {
            this.registerRoute(core, 'GUI_Pane', (msg) => this.broadcastToPanes(msg));
        }
    }

    registerRoute(from, to, handler) {
        const key = `${from}->${to}`;
        this.routes.set(key, handler);
    }

    registerBeacon(type, id, socket) {
        this.beacons.set(id, { type, id, socket, lastSeen: Date.now() });
        console.log(`[CircularRouter] Beacon registered: ${type}/${id}`);
    }

    unregisterBeacon(id) {
        this.beacons.delete(id);
    }

    routeMessage(msg) {
        const key = `${msg.source}->${msg.target}`;

        // Log for audit
        this.logMessage(msg);

        // Try direct route first
        if (this.routes.has(key)) {
            return this.routes.get(key)(msg);
        }

        // Try GUI_Pane wildcard route
        const paneKey = `GUI_Pane->${msg.target}`;
        if (this.routes.has(paneKey)) {
            return this.routes.get(paneKey)(msg);
        }

        // Fallback: circular broadcast
        return this.circularBroadcast(msg);
    }

    forwardToTarget(msg) {
        const targetBeacons = [];
        for (const [id, beacon] of this.beacons) {
            if (beacon.type === msg.target) {
                targetBeacons.push(beacon);
            }
        }

        if (targetBeacons.length === 0) {
            console.warn(`[CircularRouter] No beacon found for target: ${msg.target}`);
            return { delivered: false, reason: 'no_target_beacon' };
        }

        for (const beacon of targetBeacons) {
            if (beacon.socket && beacon.socket.readyState === 1) {
                beacon.socket.send(JSON.stringify(msg));
            }
        }

        return { delivered: true, targets: targetBeacons.length };
    }

    broadcastToPanes(msg) {
        let count = 0;
        const paneTypes = new Set(['GUI_Pane', 'PowerShell', 'Todo',
                                    'Observability', 'Training', 'ModelRouter', 'Chat']);

        for (const [id, beacon] of this.beacons) {
            if (paneTypes.has(beacon.type)) {
                if (beacon.socket && beacon.socket.readyState === 1) {
                    beacon.socket.send(JSON.stringify(msg));
                    count++;
                }
            }
        }
        return { delivered: count > 0, panes: count };
    }

    circularBroadcast(msg) {
        // Send to next in ring: IDE→Agentic→Hotpatch→Security→IDE
        const ring = ['IDE', 'Agentic', 'Hotpatch', 'Security'];
        const idx = ring.indexOf(msg.source);
        const results = [];

        // Walk the full ring starting from the next node
        for (let i = 1; i < ring.length; i++) {
            const nextType = ring[(idx + i) % ring.length];
            for (const [id, beacon] of this.beacons) {
                if (beacon.type === nextType) {
                    if (beacon.socket && beacon.socket.readyState === 1) {
                        beacon.socket.send(JSON.stringify({
                            ...msg,
                            _circularHop: i,
                            _ringPosition: nextType
                        }));
                        results.push({ hop: i, target: nextType });
                    }
                }
            }
        }

        return { delivered: results.length > 0, hops: results };
    }

    // Get status of all registered beacons
    getStatus() {
        const status = [];
        for (const [id, beacon] of this.beacons) {
            status.push({
                id: beacon.id,
                type: beacon.type,
                connected: beacon.socket && beacon.socket.readyState === 1,
                lastSeen: beacon.lastSeen
            });
        }
        return status;
    }

    // Message audit log
    logMessage(msg) {
        this.messageLog.push({
            timestamp: Date.now(),
            source: msg.source,
            target: msg.target,
            verb: msg.verb || msg.command,
            payloadSize: msg.payload ? msg.payload.length : 0
        });

        // Keep log bounded
        if (this.messageLog.length > this.maxLogSize) {
            this.messageLog = this.messageLog.slice(-this.maxLogSize / 2);
        }
    }

    getMessageLog(limit = 50) {
        return this.messageLog.slice(-limit);
    }
}

module.exports = CircularBeaconRouter;
