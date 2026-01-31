// src/engines/files.engine.js
const filesEngineV2 = {
  id: 'files',
  name: 'File Management',
  version: '1.0.0',
  icon: '📁',
  group: 'Storage',
  
  // Self-sufficiency detection
  selfSufficient: true, // JavaScript file management is self-sufficient
  externalDependencies: [],
  requiredDependencies: [],
  actions: [
    {
      method: 'GET',
      path: '/api/files',
      summary: 'List Files',
      inputs: []
    },
    {
      method: 'POST',
      path: '/api/upload',
      summary: 'Upload File',
      inputs: [
        { name: 'file', type: 'file', required: true, help: 'File to upload' }
      ]
    },
    {
      method: 'GET',
      path: '/api/download',
      summary: 'Download File',
      inputs: [
        { name: 'filename', type: 'text', required: true, help: 'Filename to download', placeholder: 'example.exe' }
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

module.exports = filesEngineV2;
