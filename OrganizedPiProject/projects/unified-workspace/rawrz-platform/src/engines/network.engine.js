// src/engines/network.engine.js
module.exports = {
  id: 'network',
  name: 'Network Tools',
  version: '1.0.0',
  icon: '🌐',
  group: 'Network',
  actions: [
    {
      method: 'GET',
      path: '/api/dns',
      summary: 'DNS Lookup',
      inputs: [
        { name: 'hostname', type: 'text', required: true, help: 'Domain name to resolve', placeholder: 'example.com' }
      ]
    },
    {
      method: 'GET',
      path: '/api/ping',
      summary: 'Ping Host',
      inputs: [
        { name: 'host', type: 'text', required: true, help: 'Host to ping', placeholder: '8.8.8.8' }
      ]
    }
  ]
};
