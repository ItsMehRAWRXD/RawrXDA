// src/engines/crypto.engine.js
const cryptoEngineV2 = {
  id: 'crypto',
  name: 'Cryptography',
  version: '2.0.0',
  icon: '🔐',
  group: 'Security',
  
  // Self-sufficiency detection
  selfSufficient: true, // JavaScript crypto is self-sufficient
  externalDependencies: [],
  requiredDependencies: [],
  actions: [
    // Version 1 APIs (Legacy)
    {
      method: 'POST',
      path: '/api/v1/hash',
      summary: 'Hash text (v1)',
      version: '1.0.0',
      deprecated: true,
      inputs: [
        { name: 'input', type: 'textarea', required: true, help: 'String to hash', maxLength: 61440 },
        { name: 'algorithm', type: 'select', options: ['sha256', 'sha512'] },
        { name: 'save', type: 'checkbox' },
        { name: 'extension', type: 'text', placeholder: 'txt' }
      ]
    },
    {
      method: 'POST',
      path: '/api/v1/encrypt',
      summary: 'Encrypt (AES v1)',
      version: '1.0.0',
      deprecated: true,
      inputs: [
        { name: 'input', type: 'textarea', required: true, help: 'Plaintext', maxLength: 61440 },
        { name: 'algorithm', type: 'select', required: true, options: ['aes-256-gcm', 'aes-128-cbc'] },
        { name: 'extension', type: 'text', placeholder: 'txt' }
      ]
    },
    {
      method: 'POST',
      path: '/api/v1/decrypt',
      summary: 'Decrypt (AES v1)',
      version: '1.0.0',
      deprecated: true,
      inputs: [
        { name: 'input', type: 'textarea', required: true, help: 'Base64 ciphertext', maxLength: 61440 },
        { name: 'algorithm', type: 'select', required: true, options: ['aes-256-gcm', 'aes-128-cbc'] },
        { name: 'key', type: 'text', required: true, help: 'Base64 key' },
        { name: 'iv', type: 'text', required: true, help: 'Base64 IV' },
        { name: 'extension', type: 'text', placeholder: 'txt' }
      ]
    },
    // Version 2 APIs (Enhanced)
    {
      method: 'POST',
      path: '/api/v2/hash',
      summary: 'Hash text (v2)',
      version: '2.0.0',
      inputs: [
        { name: 'input', type: 'textarea', required: true, help: 'String to hash', maxLength: 61440 },
        { name: 'algorithm', type: 'select', options: ['sha256', 'sha512', 'sha3-256', 'blake2b-256'] },
        { name: 'save', type: 'checkbox', help: 'Save hash to file system' },
        { name: 'extension', type: 'text', placeholder: 'txt' },
        { name: 'salt', type: 'text', help: 'Optional salt for enhanced security' },
        { name: 'iterations', type: 'number', help: 'Number of hash iterations (PBKDF2)', min: 1, max: 100000 }
      ]
    },
    {
      method: 'POST',
      path: '/api/v2/encrypt',
      summary: 'Encrypt (AES v2)',
      version: '2.0.0',
      inputs: [
        { name: 'input', type: 'textarea', required: true, help: 'Plaintext', maxLength: 61440 },
        { name: 'algorithm', type: 'select', required: true, options: ['aes-256-gcm', 'aes-128-cbc', 'aes-256-cbc', 'chacha20-poly1305'] },
        { name: 'extension', type: 'text', placeholder: 'txt' },
        { name: 'keyDerivation', type: 'select', options: ['random', 'pbkdf2', 'scrypt'], help: 'Key derivation method' },
        { name: 'password', type: 'password', help: 'Password for key derivation (if not using random keys)' }
      ]
    },
    {
      method: 'POST',
      path: '/api/v2/decrypt',
      summary: 'Decrypt (AES v2)',
      version: '2.0.0',
      inputs: [
        { name: 'input', type: 'textarea', required: true, help: 'Base64 ciphertext', maxLength: 61440 },
        { name: 'algorithm', type: 'select', required: true, options: ['aes-256-gcm', 'aes-128-cbc', 'aes-256-cbc', 'chacha20-poly1305'] },
        { name: 'key', type: 'text', required: true, help: 'Base64 key' },
        { name: 'iv', type: 'text', required: true, help: 'Base64 IV' },
        { name: 'extension', type: 'text', placeholder: 'txt' },
        { name: 'authTag', type: 'text', help: 'Authentication tag (for GCM/Poly1305)' }
      ]
    },
    // Backward compatibility endpoints (redirect to v1)
    {
      method: 'POST',
      path: '/api/hash',
      summary: 'Hash text (legacy)',
      version: '1.0.0',
      deprecated: true,
      redirectTo: '/api/v1/hash',
      inputs: [
        { name: 'input', type: 'textarea', required: true, help: 'String to hash', maxLength: 61440 },
        { name: 'algorithm', type: 'select', options: ['sha256', 'sha512'] },
        { name: 'save', type: 'checkbox' },
        { name: 'extension', type: 'text', placeholder: 'txt' }
      ]
    },
    {
      method: 'POST',
      path: '/api/encrypt',
      summary: 'Encrypt (AES legacy)',
      version: '1.0.0',
      deprecated: true,
      redirectTo: '/api/v1/encrypt',
      inputs: [
        { name: 'input', type: 'textarea', required: true, help: 'Plaintext', maxLength: 61440 },
        { name: 'algorithm', type: 'select', required: true, options: ['aes-256-gcm', 'aes-128-cbc'] },
        { name: 'extension', type: 'text', placeholder: 'txt' }
      ]
    },
    {
      method: 'POST',
      path: '/api/decrypt',
      summary: 'Decrypt (AES legacy)',
      version: '1.0.0',
      deprecated: true,
      redirectTo: '/api/v1/decrypt',
      inputs: [
        { name: 'input', type: 'textarea', required: true, help: 'Base64 ciphertext', maxLength: 61440 },
        { name: 'algorithm', type: 'select', required: true, options: ['aes-256-gcm', 'aes-128-cbc'] },
        { name: 'key', type: 'text', required: true, help: 'Base64 key' },
        { name: 'iv', type: 'text', required: true, help: 'Base64 IV' },
        { name: 'extension', type: 'text', placeholder: 'txt' }
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

module.exports = cryptoEngineV2;
