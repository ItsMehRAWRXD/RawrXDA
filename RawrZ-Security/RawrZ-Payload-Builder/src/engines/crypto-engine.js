'use strict';

// Example engine manifest for the dynamic GUI
const cryptoEngine = {
  id: 'crypto',
  name: 'Crypto',
  version: '1.0.0',
  
  // Self-sufficiency detection
  selfSufficient: true, // JavaScript crypto is self-sufficient
  externalDependencies: [],
  requiredDependencies: [],
  actions: [
    {
      method: 'POST',
      path: '/api/hash',
      summary: 'Hash text',
      inputs: [
        { name: 'input', type: 'text', required: true, help: 'String to hash', maxLength: 61440 },
        { name: 'algorithm', type: 'select', required: false, help: 'Hash algorithm', options: ['sha256','sha512'] },
        { name: 'save', type: 'checkbox', required: false, help: 'Also save blob' },
        { name: 'extension', type: 'text', required: false, help: 'Optional extension' }
      ]
    },
    {
      method: 'POST',
      path: '/api/encrypt',
      summary: 'Encrypt text',
      inputs: [
        { name: 'input', type: 'text', required: true, help: 'Plaintext', maxLength: 61440 },
        { name: 'algorithm', type: 'select', required: true, options: ['aes-256-gcm','aes-128-cbc'] },
        { name: 'extension', type: 'text', required: false, help: 'Optional extension' }
      ]
    },
    {
      method: 'POST',
      path: '/api/decrypt',
      summary: 'Decrypt text',
      inputs: [
        { name: 'input', type: 'text', required: true, help: 'Base64 ciphertext', maxLength: 61440 },
        { name: 'algorithm', type: 'select', required: true, options: ['aes-256-gcm','aes-128-cbc'] },
        { name: 'key', type: 'text', required: true, help: 'Base64 key' },
        { name: 'iv', type: 'text', required: true, help: 'Base64 IV/nonce' },
        { name: 'extension', type: 'text', required: false, help: 'Optional extension' }
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

module.exports = cryptoEngine;
