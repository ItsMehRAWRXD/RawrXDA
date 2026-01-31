'use strict';

// Logging and Monitoring Engine
module.exports = {
  id: 'logs',
  name: 'Logging & Monitoring',
  version: '1.0.0',
  icon: '📊',
  group: 'Monitoring',
  actions: [
    {
      method: 'GET',
      path: '/api/logs/recent',
      summary: 'Get Recent Logs',
      inputs: [
        { name: 'limit', type: 'number', required: false, help: 'Number of log entries to return', placeholder: '100' },
        { name: 'level', type: 'select', required: false, help: 'Filter by log level', options: ['all', 'error', 'warn', 'info', 'debug'] }
      ]
    },
    {
      method: 'GET',
      path: '/api/logs/stats',
      summary: 'Get Log Statistics',
      inputs: []
    },
    {
      method: 'POST',
      path: '/api/logs/clear',
      summary: 'Clear Log History',
      inputs: [
        { name: 'confirm', type: 'checkbox', required: true, help: 'Confirm log clearing' }
      ]
    }
  ]
};
