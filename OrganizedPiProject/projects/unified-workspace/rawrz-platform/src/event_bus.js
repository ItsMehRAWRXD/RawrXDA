// src/event_bus.js
'use strict';

const EventEmitter = require('events');

class EventBus extends EventEmitter {
  constructor() {
    super();
    this.setMaxListeners(100); // Increase limit for production use
    this.eventHistory = new Map();
    this.maxHistorySize = 1000;
  }

  // Enhanced emit with history tracking
  emit(eventName, data) {
    // Add timestamp and unique ID to event data
    const enhancedData = {
      id: this.generateEventId(),
      timestamp: new Date().toISOString(),
      event: eventName,
      data: data,
      source: 'system'
    };

    // Store in history
    this.addToHistory(eventName, enhancedData);

    // Emit the event
    return super.emit(eventName, enhancedData);
  }

  // Emit with source tracking
  emitFrom(source, eventName, data) {
    const enhancedData = {
      id: this.generateEventId(),
      timestamp: new Date().toISOString(),
      event: eventName,
      data: data,
      source: source
    };

    this.addToHistory(eventName, enhancedData);
    return super.emit(eventName, enhancedData);
  }

  // Add event to history
  addToHistory(eventName, data) {
    if (!this.eventHistory.has(eventName)) {
      this.eventHistory.set(eventName, []);
    }

    const history = this.eventHistory.get(eventName);
    history.push(data);

    // Maintain max history size
    if (history.length > this.maxHistorySize) {
      history.shift();
    }
  }

  // Get event history
  getHistory(eventName, limit = 100) {
    if (!eventName) {
      // Return all events
      const allEvents = [];
      for (const [name, events] of this.eventHistory) {
        allEvents.push(...events.map(e => ({ ...e, eventName: name })));
      }
      return allEvents
        .sort((a, b) => new Date(b.timestamp) - new Date(a.timestamp))
        .slice(0, limit);
    }

    const history = this.eventHistory.get(eventName) || [];
    return history.slice(-limit);
  }

  // Generate unique event ID
  generateEventId() {
    return `evt_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
  }

  // Subscribe to multiple events
  onMultiple(events, listener) {
    const wrappedListener = (data) => {
      listener(data.event, data);
    };

    events.forEach(event => {
      this.on(event, wrappedListener);
    });

    // Return unsubscribe function
    return () => {
      events.forEach(event => {
        this.removeListener(event, wrappedListener);
      });
    };
  }

  // Wait for specific event with timeout
  waitFor(eventName, timeout = 5000) {
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.removeListener(eventName, handler);
        reject(new Error(`Timeout waiting for event: ${eventName}`));
      }, timeout);

      const handler = (data) => {
        clearTimeout(timer);
        resolve(data);
      };

      this.once(eventName, handler);
    });
  }

  // Get event statistics
  getStats() {
    const stats = {
      totalEvents: 0,
      eventTypes: {},
      listeners: this.eventNames().length,
      historySize: 0
    };

    for (const [eventName, history] of this.eventHistory) {
      stats.totalEvents += history.length;
      stats.eventTypes[eventName] = history.length;
    }

    stats.historySize = this.eventHistory.size;
    return stats;
  }

  // Clear history
  clearHistory(eventName) {
    if (eventName) {
      this.eventHistory.delete(eventName);
    } else {
      this.eventHistory.clear();
    }
  }

  // Health check
  health() {
    return {
      status: 'healthy',
      listeners: this.eventNames().length,
      historySize: this.eventHistory.size,
      maxListeners: this.getMaxListeners(),
      currentListeners: this.listenerCount()
    };
  }
}

// Create singleton instance
const eventBus = new EventBus();

// Add some common event types for RawrZ platform
const COMMON_EVENTS = {
  // System events
  SYSTEM_STARTUP: 'system.startup',
  SYSTEM_SHUTDOWN: 'system.shutdown',
  SYSTEM_ERROR: 'system.error',

  // Engine events
  ENGINE_LOADED: 'engine.loaded',
  ENGINE_ERROR: 'engine.error',
  ENGINE_EXECUTED: 'engine.executed',

  // API events
  API_REQUEST: 'api.request',
  API_RESPONSE: 'api.response',
  API_ERROR: 'api.error',

  // Authentication events
  AUTH_LOGIN: 'auth.login',
  AUTH_LOGOUT: 'auth.logout',
  AUTH_FAILED: 'auth.failed',
  AUTH_TOKEN_REFRESH: 'auth.token_refresh',

  // File operations
  FILE_UPLOADED: 'file.uploaded',
  FILE_DOWNLOADED: 'file.downloaded',
  FILE_DELETED: 'file.deleted',

  // Workflow events
  WORKFLOW_STARTED: 'workflow.started',
  WORKFLOW_COMPLETED: 'workflow.completed',
  WORKFLOW_FAILED: 'workflow.failed',
  WORKFLOW_STEP_COMPLETED: 'workflow.step_completed',

  // Database events
  DB_QUERY_EXECUTED: 'db.query_executed',
  DB_CONNECTION_LOST: 'db.connection_lost',
  DB_CONNECTION_RESTORED: 'db.connection_restored',

  // Security events
  SECURITY_VIOLATION: 'security.violation',
  RATE_LIMIT_EXCEEDED: 'rate_limit.exceeded',
  SUSPICIOUS_ACTIVITY: 'security.suspicious_activity'
};

// Export both the instance and event constants
module.exports = {
  eventBus,
  COMMON_EVENTS,
  EventBus // Export class for testing
};
