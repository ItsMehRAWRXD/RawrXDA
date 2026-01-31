'use strict';

// Asynchronous Task Management Engine
module.exports = {
  id: 'queue',
  name: 'Task Queue',
  version: '1.0.0',
  icon: '⏳',
  group: 'Processing',
  actions: [
    {
      method: 'GET',
      path: '/api/queue/status',
      summary: 'Get Queue Status',
      inputs: []
    },
    {
      method: 'GET',
      path: '/api/queue/jobs',
      summary: 'List Active Jobs',
      inputs: [
        { name: 'status', type: 'select', required: false, help: 'Filter by job status', options: ['all', 'pending', 'running', 'completed', 'failed'] }
      ]
    },
    {
      method: 'POST',
      path: '/api/queue/job',
      summary: 'Create New Job',
      inputs: [
        { name: 'type', type: 'select', required: true, help: 'Job type', options: ['crypto', 'network', 'file', 'system'] },
        { name: 'action', type: 'text', required: true, help: 'Action to perform' },
        { name: 'data', type: 'textarea', required: false, help: 'Job data (JSON)' }
      ]
    },
    {
      method: 'GET',
      path: '/api/queue/job/:id',
      summary: 'Get Job Details',
      inputs: [
        { name: 'id', type: 'text', required: true, help: 'Job ID' }
      ]
    },
    {
      method: 'DELETE',
      path: '/api/queue/job/:id',
      summary: 'Cancel Job',
      inputs: [
        { name: 'id', type: 'text', required: true, help: 'Job ID to cancel' }
      ]
    }
  ]
};
