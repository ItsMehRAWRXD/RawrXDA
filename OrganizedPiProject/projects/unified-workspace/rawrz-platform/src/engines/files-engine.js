'use strict';

// Example files engine manifest
const filesEngine = {
  id: 'files',
  name: 'Files',
  version: '1.0.0',
  
  // Self-sufficiency detection
  selfSufficient: true, // JavaScript file operations are self-sufficient
  externalDependencies: [],
  requiredDependencies: [],
  actions: [
    {
      method: 'GET',
      path: '/api/files',
      summary: 'List uploaded files',
      inputs: []
    },
    {
      method: 'POST',
      path: '/api/upload',
      summary: 'Upload file',
      inputs: [
        { name: 'file', type: 'file', required: true, help: 'File to upload' }
      ]
    },
    {
      method: 'GET',
      path: '/api/download',
      summary: 'Download file',
      inputs: [
        { name: 'filename', type: 'text', required: true, help: 'Filename to download' }
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

module.exports = filesEngine;
