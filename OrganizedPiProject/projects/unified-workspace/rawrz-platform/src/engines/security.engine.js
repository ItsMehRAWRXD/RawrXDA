'use strict';

// Security and Authentication Engine
module.exports = {
  id: 'security',
  name: 'Security & Auth',
  version: '1.0.0',
  icon: '🔒',
  group: 'Security',
  actions: [
    {
      method: 'GET',
      path: '/api/security/token-info',
      summary: 'Get Token Information',
      inputs: []
    },
    {
      method: 'POST',
      path: '/api/security/validate-token',
      summary: 'Validate Authentication Token',
      inputs: [
        { name: 'token', type: 'text', required: true, help: 'Token to validate' }
      ]
    },
    {
      method: 'GET',
      path: '/api/security/permissions',
      summary: 'Get User Permissions',
      inputs: []
    },
    {
      method: 'POST',
      path: '/api/security/audit-log',
      summary: 'Get Security Audit Log',
      inputs: [
        { name: 'limit', type: 'number', required: false, help: 'Number of entries to return', placeholder: '100' },
        { name: 'level', type: 'select', required: false, help: 'Filter by security level', options: ['all', 'info', 'warn', 'error', 'critical'] }
      ]
    }
  ]
};
