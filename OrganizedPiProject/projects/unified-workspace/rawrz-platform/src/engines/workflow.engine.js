'use strict';

// Workflow Automation Engine
module.exports = {
  id: 'workflow',
  name: 'Workflow Automation',
  version: '1.0.0',
  icon: '⚡',
  group: 'Automation',
  actions: [
    {
      method: 'POST',
      path: '/api/workflow/execute',
      summary: 'Execute Multi-Step Workflow',
      inputs: [
        { name: 'name', type: 'text', required: true, help: 'Workflow name', placeholder: 'My Workflow' },
        { name: 'steps', type: 'textarea', required: true, help: 'JSON array of workflow steps', maxLength: 50000 },
        { name: 'parallel', type: 'checkbox', required: false, help: 'Execute steps in parallel' }
      ]
    },
    {
      method: 'GET',
      path: '/api/workflow/templates',
      summary: 'Get Workflow Templates',
      inputs: []
    },
    {
      method: 'POST',
      path: '/api/workflow/validate',
      summary: 'Validate Workflow Definition',
      inputs: [
        { name: 'steps', type: 'textarea', required: true, help: 'JSON array of workflow steps to validate' }
      ]
    },
    {
      method: 'GET',
      path: '/api/workflow/history',
      summary: 'Get Workflow Execution History',
      inputs: [
        { name: 'limit', type: 'number', required: false, help: 'Number of executions to return', placeholder: '50' }
      ]
    }
  ]
};
