'use strict';

// Configuration Management Engine
const configEngine = {
  id: 'config',
  name: 'Configuration',
  version: '1.0.0',
  icon: '⚙️',
  group: 'System',
  
  // Self-sufficiency detection
  selfSufficient: true, // JavaScript configuration is self-sufficient
  externalDependencies: [],
  requiredDependencies: [],
  actions: [
    {
      method: 'GET',
      path: '/api/config/engines',
      summary: 'List Engine Configuration',
      inputs: []
    },
    {
      method: 'POST',
      path: '/api/config/engines/toggle',
      summary: 'Enable/Disable Engine',
      inputs: [
        { name: 'engineId', type: 'text', required: true, help: 'Engine ID to toggle' },
        { name: 'enabled', type: 'checkbox', required: true, help: 'Enable or disable the engine' }
      ]
    },
    {
      method: 'GET',
      path: '/api/config/settings',
      summary: 'Get System Settings',
      inputs: []
    },
    {
      method: 'POST',
      path: '/api/config/settings',
      summary: 'Update System Settings',
      inputs: [
        { name: 'setting', type: 'text', required: true, help: 'Setting name' },
        { name: 'value', type: 'text', required: true, help: 'Setting value' }
      ]
    }
  ],

  // Method to check if engine is self-sufficient
  isSelfSufficient() {
    return this.selfSufficient;
  },

  // Method to get dependency information
  getDependencyInfo() {
    return {
      selfSufficient: this.selfSufficient,
      externalDependencies: this.externalDependencies,
      requiredDependencies: this.requiredDependencies,
      hasExternalDependencies: this.externalDependencies.length > 0,
      hasRequiredDependencies: this.requiredDependencies.length > 0
    };
  }
};

module.exports = configEngine;
