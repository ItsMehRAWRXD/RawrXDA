// ─────────────────────────────────────────────────────────────────────────────
// RawrXD Beacon Manager — Circular Component Communication System
// Enables bidirectional messaging between all components: IDE, Security, Encryption, Java, etc.
// ─────────────────────────────────────────────────────────────────────────────
'use strict';

const EventEmitter = require('events');

class BeaconManager extends EventEmitter {
  constructor() {
    super();
    this.beacons = new Map(); // componentId -> { type, address, status, lastSeen }
    this.messages = new Map(); // messageId -> message
    this.routes = new Map(); // sourceType -> targetTypes[]
    this.stats = { messagesSent: 0, messagesReceived: 0, beaconsRegistered: 0 };
    this.startTime = Date.now();

    // Initialize circular routing for known components
    this.initializeCircularRouting();

    console.log('[BeaconManager] ✅ Initialized with circular routing');
  }

  initializeCircularRouting() {
    // Define component types and their interconnections
    const components = {
      'ide': ['security', 'encryption', 'java', 'agentic', 'hotpatch'],
      'security': ['ide', 'encryption', 'java', 'agentic'],
      'encryption': ['ide', 'security', 'java', 'carmilla'],
      'java': ['ide', 'security', 'encryption', 'rawrz'],
      'agentic': ['ide', 'security', 'hotpatch', 'autonomous'],
      'hotpatch': ['ide', 'agentic', 'security'],
      'carmilla': ['encryption', 'security'],
      'rawrz': ['java', 'security'],
      'autonomous': ['agentic', 'ide']
    };

    // Set bidirectional routes
    for (const [source, targets] of Object.entries(components)) {
      this.routes.set(source, targets);
      // Ensure reverse routes for circularity
      for (const target of targets) {
        if (!this.routes.has(target)) {
          this.routes.set(target, []);
        }
        if (!this.routes.get(target).includes(source)) {
          this.routes.get(target).push(source);
        }
      }
    }
  }

  registerBeacon(componentId, type, address = null) {
    this.beacons.set(componentId, {
      type,
      address,
      status: 'active',
      registeredAt: Date.now(),
      lastSeen: Date.now()
    });
    this.stats.beaconsRegistered++;
    this.emit('beacon-registered', { componentId, type, address });
    console.log(`[BeaconManager] 📡 Registered beacon: ${componentId} (${type})`);
  }

  unregisterBeacon(componentId) {
    if (this.beacons.has(componentId)) {
      const beacon = this.beacons.get(componentId);
      this.beacons.delete(componentId);
      this.emit('beacon-unregistered', { componentId, type: beacon.type });
      console.log(`[BeaconManager] 📡 Unregistered beacon: ${componentId}`);
    }
  }

  updateBeaconStatus(componentId, status) {
    if (this.beacons.has(componentId)) {
      this.beacons.get(componentId).lastSeen = Date.now();
      this.beacons.get(componentId).status = status;
    }
  }

  async sendMessage(sourceId, targetType, message, options = {}) {
    const messageId = `${sourceId}_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
    const fullMessage = {
      id: messageId,
      sourceId,
      targetType,
      payload: message,
      timestamp: Date.now(),
      ttl: options.ttl || 30000, // 30 seconds default
      priority: options.priority || 'normal'
    };

    this.messages.set(messageId, fullMessage);
    this.stats.messagesSent++;

    // Route to all beacons of target type
    const delivered = [];
    for (const [componentId, beacon] of this.beacons) {
      if (beacon.type === targetType && beacon.status === 'active') {
        try {
          await this.deliverMessage(componentId, fullMessage);
          delivered.push(componentId);
        } catch (error) {
          console.warn(`[BeaconManager] Failed to deliver to ${componentId}:`, error.message);
        }
      }
    }

    // Also route via circular connections
    const sourceBeacon = this.beacons.get(sourceId);
    if (sourceBeacon) {
      const connectedTypes = this.routes.get(sourceBeacon.type) || [];
      if (connectedTypes.includes(targetType)) {
        // Direct route exists
      } else {
        // Try indirect routing through connected components
        for (const connectedType of connectedTypes) {
          if (connectedType !== targetType) {
            for (const [componentId, beacon] of this.beacons) {
              if (beacon.type === connectedType && beacon.status === 'active') {
                try {
                  const routedMessage = { ...fullMessage, routedVia: connectedType };
                  await this.deliverMessage(componentId, routedMessage);
                  delivered.push(`${componentId}(routed)`);
                } catch (error) {
                  // Ignore routing failures
                }
              }
            }
          }
        }
      }
    }

    this.emit('message-sent', { messageId, delivered: delivered.length });
    return { messageId, delivered };
  }

  async deliverMessage(componentId, message) {
    const beacon = this.beacons.get(componentId);
    if (!beacon) {
      throw new Error(`Beacon ${componentId} not found`);
    }

    // For local delivery (same process), emit event
    if (!beacon.address) {
      this.emit('message-received', { componentId, message });
      this.stats.messagesReceived++;
      return;
    }

    // For remote delivery, could implement HTTP/WebSocket
    // For now, assume local
    this.emit('message-received', { componentId, message });
    this.stats.messagesReceived++;
  }

  getStatus() {
    return {
      beacons: Array.from(this.beacons.entries()).map(([id, b]) => ({
        id, type: b.type, status: b.status, lastSeen: b.lastSeen
      })),
      routes: Object.fromEntries(this.routes),
      stats: this.stats,
      uptime: Math.round((Date.now() - this.startTime) / 1000)
    };
  }

  // Specific beacon types for components
  async sendToAgentic(sourceId, action, params = {}) {
    return this.sendMessage(sourceId, 'agentic', { action, params });
  }

  async sendToHotpatch(sourceId, target, patch) {
    return this.sendMessage(sourceId, 'hotpatch', { target, patch });
  }

  async sendToSecurity(sourceId, operation, data) {
    return this.sendMessage(sourceId, 'security', { operation, data });
  }

  async sendToEncryption(sourceId, mode, data) {
    return this.sendMessage(sourceId, 'encryption', { mode, data });
  }
}

module.exports = new BeaconManager();