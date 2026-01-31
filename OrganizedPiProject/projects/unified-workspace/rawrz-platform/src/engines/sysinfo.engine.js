'use strict';

// System Information Engine
module.exports = {
  id: 'sysinfo',
  name: 'System Information',
  version: '1.0.0',
  icon: '🖥️',
  group: 'System',
  actions: [
    {
      method: 'GET',
      path: '/api/sysinfo/os',
      summary: 'Get Operating System Info',
      inputs: []
    },
    {
      method: 'GET',
      path: '/api/sysinfo/cpu',
      summary: 'Get CPU Information',
      inputs: []
    },
    {
      method: 'GET',
      path: '/api/sysinfo/memory',
      summary: 'Get Memory Information',
      inputs: []
    },
    {
      method: 'GET',
      path: '/api/sysinfo/network',
      summary: 'Get Network Interfaces',
      inputs: []
    },
    {
      method: 'GET',
      path: '/api/sysinfo/uptime',
      summary: 'Get System Uptime',
      inputs: []
    }
  ]
};
