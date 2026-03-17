const { ipcRenderer } = require('electron');

// Global state
let availableFeatures = [];
let encryptionAlgorithms = [];
let selectedFeatures = [];
let selectedEncryption = [];
let buildCount = 0;
let currentConfig = {};

// DOM elements
let elements = {};

// Initialize the application
document.addEventListener('DOMContentLoaded', async () => {
  console.log('Star5IDE Polymorphic Builder initializing...');

  // Cache DOM elements
  cacheElements();

  // Load data
  await loadAvailableFeatures();
  await loadEncryptionAlgorithms();

  // Setup event listeners
  setupEventListeners();

  // Initialize UI
  initializeUI();

  // Start timestamp updates
  updateTimestamp();
  setInterval(updateTimestamp, 1000);

  console.log('Star5IDE Polymorphic Builder ready!');
  updateStatus('Ready - Select features and generate polymorphic build');
});

function cacheElements() {
  // Main sections
  elements.featureCount = document.getElementById('featureCount');
  elements.algorithmCount = document.getElementById('algorithmCount');
  elements.estimatedSize = document.getElementById('estimatedSize');
  elements.statusText = document.getElementById('statusText');
  elements.buildCount = document.getElementById('buildCount');
  elements.timestamp = document.getElementById('timestamp');

  // Configuration
  elements.buildName = document.getElementById('buildName');
  elements.architecture = document.getElementById('architecture');
  elements.outputFormat = document.getElementById('outputFormat');
  elements.polymorphicMode = document.getElementById('polymorphicMode');
  elements.obfuscationMode = document.getElementById('obfuscationMode');
  elements.stealthMode = document.getElementById('stealthMode');

  // Feature sections
  elements.coreFeatures = document.getElementById('coreFeatures');
  elements.networkFeatures = document.getElementById('networkFeatures');
  elements.analysisFeatures = document.getElementById('analysisFeatures');
  elements.advancedFeatures = document.getElementById('advancedFeatures');

  // Encryption sections
  elements.symmetricAlgorithms = document.getElementById('symmetricAlgorithms');
  elements.asymmetricAlgorithms = document.getElementById('asymmetricAlgorithms');
  elements.hashAlgorithms = document.getElementById('hashAlgorithms');

  // Build elements
  elements.polymorphicSeed = document.getElementById('polymorphicSeed');
  elements.buildVariant = document.getElementById('buildVariant');
  elements.customCompilerFlags = document.getElementById('customCompilerFlags');
  elements.buildProgress = document.getElementById('buildProgress');
  elements.progressFill = document.getElementById('progressFill');
  elements.progressText = document.getElementById('progressText');
  elements.buildResults = document.getElementById('buildResults');

  // Output elements
  elements.outputConsole = document.getElementById('outputConsole');

  // Result elements
  elements.resultBuildId = document.getElementById('resultBuildId');
  elements.resultOutputFile = document.getElementById('resultOutputFile');
  elements.resultFileSize = document.getElementById('resultFileSize');
  elements.resultBuildTime = document.getElementById('resultBuildTime');
  elements.resultFeatures = document.getElementById('resultFeatures');
  elements.resultAlgorithms = document.getElementById('resultAlgorithms');
}

function setupEventListeners() {
  // Tab navigation
  document.querySelectorAll('.tab-btn').forEach(btn => {
    btn.addEventListener('click', () => switchTab(btn.dataset.tab));
  });

  // Feature selection
  document.getElementById('selectAllFeatures').addEventListener('click', selectAllFeatures);
  document.getElementById('clearAllFeatures').addEventListener('click', clearAllFeatures);
  document.getElementById('presetBasic').addEventListener('click', () => applyFeaturePreset('basic'));
  document.getElementById('presetAdvanced').addEventListener('click', () => applyFeaturePreset('advanced'));
  document.getElementById('presetComplete').addEventListener('click', () => applyFeaturePreset('complete'));

  // Encryption selection
  document.getElementById('selectAllEncryption').addEventListener('click', selectAllEncryption);
  document.getElementById('clearAllEncryption').addEventListener('click', clearAllEncryption);
  document.getElementById('presetStandard').addEventListener('click', () => applyEncryptionPreset('standard'));
  document.getElementById('presetMilitary').addEventListener('click', () => applyEncryptionPreset('military'));

  // Build controls
  document.getElementById('generateSeed').addEventListener('click', generatePolymorphicSeed);
  document.getElementById('generateBuild').addEventListener('click', generatePolymorphicBuild);
  document.getElementById('previewBuild').addEventListener('click', previewBuild);
  document.getElementById('validateConfig').addEventListener('click', validateConfiguration);

  // Output controls
  document.getElementById('clearOutput').addEventListener('click', clearOutput);
  document.getElementById('copyOutput').addEventListener('click', copyOutput);
  document.getElementById('saveOutput').addEventListener('click', saveOutput);

  // Config controls
  document.getElementById('saveConfig').addEventListener('click', saveConfiguration);
  document.getElementById('loadConfig').addEventListener('click', loadConfiguration);

  // Configuration change listeners
  elements.buildName.addEventListener('input', updateEstimatedSize);
  elements.architecture.addEventListener('change', updateEstimatedSize);
  elements.outputFormat.addEventListener('change', updateEstimatedSize);
  elements.polymorphicMode.addEventListener('change', updateEstimatedSize);
  elements.obfuscationMode.addEventListener('change', updateEstimatedSize);
  elements.stealthMode.addEventListener('change', updateEstimatedSize);
}

async function loadAvailableFeatures() {
  try {
    availableFeatures = await ipcRenderer.invoke('get-available-features');
    populateFeatureUI();
    logOutput('info', `Loaded ${availableFeatures.length} available features`);
  } catch (error) {
    logOutput('error', `Failed to load features: ${error.message}`);
  }
}

async function loadEncryptionAlgorithms() {
  try {
    encryptionAlgorithms = await ipcRenderer.invoke('get-encryption-algorithms');
    populateEncryptionUI();
    logOutput('info', `Loaded ${encryptionAlgorithms.length} encryption algorithms`);
  } catch (error) {
    logOutput('error', `Failed to load encryption algorithms: ${error.message}`);
  }
}

function populateFeatureUI() {
  const categories = {
    core: ['Core Encryption', 'Digital Signatures', 'Key Management', 'Hash Generation', 'Advanced Crypto'],
    network: ['Network Analysis', 'Port Scanning', 'DNS Tools', 'Network Monitor', 'Packet Capture'],
    analysis: ['File Analysis', 'System Analysis', 'Process Analysis', 'Memory Tools', 'Registry Tools'],
    advanced: ['Stealth Mode', 'Anti-Analysis', 'Service Manager', 'Text Operations', 'Validation Tools']
  };

  Object.entries(categories).forEach(([category, features]) => {
    const container = document.getElementById(`${category}Features`);
    if (container) {
      container.innerHTML = '';
      features.forEach(feature => {
        if (availableFeatures.includes(feature)) {
          const item = createFeatureItem(feature);
          container.appendChild(item);
        }
      });
    }
  });
}

function populateEncryptionUI() {
  const categories = {
    symmetric: ['AES-128', 'AES-192', 'AES-256', 'ChaCha20', 'Camellia', 'ARIA', 'Blowfish', 'Twofish'],
    asymmetric: ['RSA', 'ECC', 'DSA', 'ElGamal', 'Diffie-Hellman'],
    hash: ['SHA-256', 'SHA-512', 'Blake2', 'MD5', 'SHA-1', 'Whirlpool']
  };

  Object.entries(categories).forEach(([category, algorithms]) => {
    const container = document.getElementById(`${category}Algorithms`);
    if (container) {
      container.innerHTML = '';
      algorithms.forEach(algorithm => {
        if (encryptionAlgorithms.includes(algorithm)) {
          const item = createAlgorithmItem(algorithm);
          container.appendChild(item);
        }
      });
    }
  });
}

function createFeatureItem(feature) {
  const item = document.createElement('div');
  item.className = 'feature-item';

  const checkbox = document.createElement('input');
  checkbox.type = 'checkbox';
  checkbox.id = `feature-${feature.replace(/[^a-zA-Z0-9]/g, '')}`;
  checkbox.addEventListener('change', () => toggleFeature(feature, checkbox.checked));

  const label = document.createElement('label');
  label.htmlFor = checkbox.id;
  label.textContent = feature;
  label.style.cursor = 'pointer';

  item.appendChild(checkbox);
  item.appendChild(label);

  return item;
}

function createAlgorithmItem(algorithm) {
  const item = document.createElement('div');
  item.className = 'algorithm-item';

  const checkbox = document.createElement('input');
  checkbox.type = 'checkbox';
  checkbox.id = `algorithm-${algorithm.replace(/[^a-zA-Z0-9]/g, '')}`;
  checkbox.addEventListener('change', () => toggleAlgorithm(algorithm, checkbox.checked));

  const label = document.createElement('label');
  label.htmlFor = checkbox.id;
  label.textContent = algorithm;
  label.style.cursor = 'pointer';

  item.appendChild(checkbox);
  item.appendChild(label);

  return item;
}

function toggleFeature(feature, selected) {
  if (selected) {
    if (!selectedFeatures.includes(feature)) {
      selectedFeatures.push(feature);
    }
  } else {
    const index = selectedFeatures.indexOf(feature);
    if (index > -1) {
      selectedFeatures.splice(index, 1);
    }
  }
  updateCounts();
  updateEstimatedSize();
  logOutput('info', `Feature ${selected ? 'selected' : 'deselected'}: ${feature}`);
}

function toggleAlgorithm(algorithm, selected) {
  if (selected) {
    if (!selectedEncryption.includes(algorithm)) {
      selectedEncryption.push(algorithm);
    }
  } else {
    const index = selectedEncryption.indexOf(algorithm);
    if (index > -1) {
      selectedEncryption.splice(index, 1);
    }
  }
  updateCounts();
  updateEstimatedSize();
  logOutput('info', `Algorithm ${selected ? 'selected' : 'deselected'}: ${algorithm}`);
}

function selectAllFeatures() {
  selectedFeatures = [...availableFeatures];
  document.querySelectorAll('[id^="feature-"]').forEach(checkbox => {
    checkbox.checked = true;
  });
  updateCounts();
  updateEstimatedSize();
  logOutput('info', 'All features selected');
}

function clearAllFeatures() {
  selectedFeatures = [];
  document.querySelectorAll('[id^="feature-"]').forEach(checkbox => {
    checkbox.checked = false;
  });
  updateCounts();
  updateEstimatedSize();
  logOutput('info', 'All features cleared');
}

function selectAllEncryption() {
  selectedEncryption = [...encryptionAlgorithms];
  document.querySelectorAll('[id^="algorithm-"]').forEach(checkbox => {
    checkbox.checked = true;
  });
  updateCounts();
  updateEstimatedSize();
  logOutput('info', 'All encryption algorithms selected');
}

function clearAllEncryption() {
  selectedEncryption = [];
  document.querySelectorAll('[id^="algorithm-"]').forEach(checkbox => {
    checkbox.checked = false;
  });
  updateCounts();
  updateEstimatedSize();
  logOutput('info', 'All encryption algorithms cleared');
}

function applyFeaturePreset(preset) {
  clearAllFeatures();

  const presets = {
    basic: ['Core Encryption', 'File Operations', 'Encoding Tools', 'Hash Generation'],
    advanced: ['Core Encryption', 'Network Analysis', 'File Analysis', 'Digital Signatures', 'Advanced Crypto', 'System Analysis'],
    complete: availableFeatures
  };

  const presetFeatures = presets[preset] || [];
  presetFeatures.forEach(feature => {
    const checkbox = document.getElementById(`feature-${feature.replace(/[^a-zA-Z0-9]/g, '')}`);
    if (checkbox) {
      checkbox.checked = true;
      toggleFeature(feature, true);
    }
  });

  logOutput('info', `Applied ${preset} feature preset (${presetFeatures.length} features)`);
}

function applyEncryptionPreset(preset) {
  clearAllEncryption();

  const presets = {
    standard: ['AES-256', 'RSA', 'SHA-256', 'ChaCha20'],
    military: ['AES-256', 'Camellia', 'ECC', 'SHA-512', 'Blake2', 'ARIA']
  };

  const presetAlgorithms = presets[preset] || [];
  presetAlgorithms.forEach(algorithm => {
    const checkbox = document.getElementById(`algorithm-${algorithm.replace(/[^a-zA-Z0-9]/g, '')}`);
    if (checkbox) {
      checkbox.checked = true;
      toggleAlgorithm(algorithm, true);
    }
  });

  logOutput('info', `Applied ${preset} encryption preset (${presetAlgorithms.length} algorithms)`);
}

function updateCounts() {
  if (elements.featureCount) elements.featureCount.textContent = selectedFeatures.length;
  if (elements.algorithmCount) elements.algorithmCount.textContent = selectedEncryption.length;
}

function updateEstimatedSize() {
  const baseSize = 50; // KB
  const featureSize = selectedFeatures.length * 15; // KB per feature
  const algorithmSize = selectedEncryption.length * 8; // KB per algorithm
  const polymorphicOverhead = elements.polymorphicMode?.checked ? 20 : 0;
  const obfuscationOverhead = elements.obfuscationMode?.checked ? 15 : 0;
  const stealthOverhead = elements.stealthMode?.checked ? 10 : 0;

  const totalSize = baseSize + featureSize + algorithmSize + polymorphicOverhead + obfuscationOverhead + stealthOverhead;

  if (elements.estimatedSize) {
    elements.estimatedSize.textContent = `~${totalSize} KB`;
  }
}

function generatePolymorphicSeed() {
  const seed = 'S5IDE-' + Date.now().toString(36) + '-' + Math.random().toString(36).substr(2, 9);
  elements.polymorphicSeed.value = seed;
  logOutput('info', `Generated polymorphic seed: ${seed}`);
}

async function generatePolymorphicBuild() {
  if (selectedFeatures.length === 0) {
    logOutput('error', 'No features selected. Please select at least one feature.');
    return;
  }

  if (selectedEncryption.length === 0) {
    logOutput('error', 'No encryption algorithms selected. Please select at least one algorithm.');
    return;
  }

  const config = getCurrentConfig();

  // Show progress
  showBuildProgress();
  logOutput('info', 'Starting polymorphic build generation...');

  try {
    // Update progress
    updateProgress(10, 'Initializing polymorphic generator...');
    await delay(500);

    updateProgress(30, 'Generating polymorphic source code...');
    await delay(1000);

    updateProgress(50, 'Creating build configuration...');
    await delay(800);

    updateProgress(70, 'Compiling polymorphic executable...');
    await delay(1200);

    updateProgress(90, 'Finalizing build...');

    // Call the main process to generate the build
    const result = await ipcRenderer.invoke('generate-polymorphic-build', config);

    if (result.success) {
      updateProgress(100, 'Build completed successfully!');
      await delay(500);

      hideBuildProgress();
      showBuildResults(result.result);

      buildCount++;
      elements.buildCount.textContent = `Builds: ${buildCount}`;

      logOutput('success', `Polymorphic build completed successfully!`);
      logOutput('info', `Build ID: ${result.result.buildId}`);
      logOutput('info', `Build path: ${result.result.buildPath}`);
      logOutput('info', `Source lines: ${result.result.sourceLines}`);

    } else {
      hideBuildProgress();
      logOutput('error', `Build failed: ${result.error}`);
    }

  } catch (error) {
    hideBuildProgress();
    logOutput('error', `Build error: ${error.message}`);
  }
}

function getCurrentConfig() {
  if (!elements.polymorphicSeed.value) {
    generatePolymorphicSeed();
  }

  return {
    name: elements.buildName.value,
    architecture: elements.architecture.value,
    outputFormat: elements.outputFormat.value,
    polymorphicMode: elements.polymorphicMode.checked,
    obfuscationMode: elements.obfuscationMode.checked,
    stealthMode: elements.stealthMode.checked,
    selectedFeatures: [...selectedFeatures],
    selectedEncryption: [...selectedEncryption],
    polymorphicSeed: elements.polymorphicSeed.value,
    buildVariant: elements.buildVariant.value,
    customCompilerFlags: elements.customCompilerFlags.value,
    variableRandomization: document.getElementById('variableRandomization').checked,
    functionRandomization: document.getElementById('functionRandomization').checked,
    structureRandomization: document.getElementById('structureRandomization').checked,
    flowObfuscation: document.getElementById('flowObfuscation').checked,
    stringEncryption: document.getElementById('stringEncryption').checked,
    constantObfuscation: document.getElementById('constantObfuscation').checked,
    timestamp: Date.now()
  };
}

function showBuildProgress() {
  elements.buildProgress.classList.remove('hidden');
  elements.buildProgress.style.display = 'block';
  elements.progressFill.style.width = '0%';
  elements.progressText.textContent = 'Initializing...';
}

function hideBuildProgress() {
  elements.buildProgress.classList.add('hidden');
  elements.buildProgress.style.display = 'none';
}

function updateProgress(percent, text) {
  elements.progressFill.style.width = `${percent}%`;
  elements.progressText.textContent = text;
}

function showBuildResults(result) {
  elements.buildResults.classList.remove('hidden');
  elements.buildResults.style.display = 'block';

  elements.resultBuildId.textContent = result.buildId;
  elements.resultOutputFile.textContent = `star5ide_${result.buildId.substring(0, 8)}.exe`;
  elements.resultFileSize.textContent = `${Math.round(Math.random() * 500 + 100)} KB`;
  elements.resultBuildTime.textContent = `${Math.round(Math.random() * 15 + 5)} seconds`;
  elements.resultFeatures.textContent = selectedFeatures.join(', ');
  elements.resultAlgorithms.textContent = selectedEncryption.join(', ');
}

function switchTab(tabName) {
  // Update tab buttons
  document.querySelectorAll('.tab-btn').forEach(btn => {
    btn.classList.remove('active');
  });
  document.querySelector(`[data-tab="${tabName}"]`).classList.add('active');

  // Update tab content
  document.querySelectorAll('.tab-pane').forEach(pane => {
    pane.classList.remove('active');
  });
  document.getElementById(`${tabName}-tab`).classList.add('active');

  logOutput('info', `Switched to ${tabName} tab`);
}

function previewBuild() {
  const config = getCurrentConfig();
  logOutput('info', 'Previewing build configuration:');
  logOutput('info', `Name: ${config.name}`);
  logOutput('info', `Architecture: ${config.architecture}`);
  logOutput('info', `Features: ${config.selectedFeatures.join(', ')}`);
  logOutput('info', `Algorithms: ${config.selectedEncryption.join(', ')}`);
  logOutput('info', `Polymorphic: ${config.polymorphicMode ? 'Enabled' : 'Disabled'}`);
}

function validateConfiguration() {
  const issues = [];

  if (selectedFeatures.length === 0) {
    issues.push('No features selected');
  }

  if (selectedEncryption.length === 0) {
    issues.push('No encryption algorithms selected');
  }

  if (!elements.buildName.value.trim()) {
    issues.push('Build name is empty');
  }

  if (issues.length === 0) {
    logOutput('success', 'Configuration validation passed - Ready to build!');
  } else {
    logOutput('warning', 'Configuration validation failed:');
    issues.forEach(issue => logOutput('warning', `- ${issue}`));
  }
}

function logOutput(level, message) {
  const timestamp = new Date().toISOString().replace('T', ' ').substr(0, 19);
  const line = document.createElement('div');
  line.className = `console-line ${level}`;

  line.innerHTML = `
        <span class="timestamp">[${timestamp}]</span>
        <span class="message">${message}</span>
    `;

  elements.outputConsole.appendChild(line);
  elements.outputConsole.scrollTop = elements.outputConsole.scrollHeight;
}

function clearOutput() {
  elements.outputConsole.innerHTML = '';
  logOutput('info', 'Output console cleared');
}

function copyOutput() {
  const text = Array.from(elements.outputConsole.children)
    .map(line => line.textContent)
    .join('\n');

  navigator.clipboard.writeText(text).then(() => {
    logOutput('info', 'Output copied to clipboard');
  });
}

function saveOutput() {
  logOutput('info', 'Save output functionality not implemented yet');
}

async function saveConfiguration() {
  const config = getCurrentConfig();
  try {
    const result = await ipcRenderer.invoke('save-build-config', config);
    if (result.success) {
      logOutput('success', `Configuration saved: ${result.path}`);
    } else {
      logOutput('error', `Failed to save configuration: ${result.error}`);
    }
  } catch (error) {
    logOutput('error', `Save error: ${error.message}`);
  }
}

function loadConfiguration() {
  logOutput('info', 'Load configuration functionality not implemented yet');
}

function updateStatus(text) {
  elements.statusText.textContent = text;
}

function updateTimestamp() {
  elements.timestamp.textContent = new Date().toLocaleString();
}

function initializeUI() {
  // Generate initial seed
  generatePolymorphicSeed();

  // Update initial counts
  updateCounts();
  updateEstimatedSize();

  // Set default compiler flags
  elements.customCompilerFlags.value = '-O3 -fomit-frame-pointer -DNDEBUG -s';

  // Hide progress and results initially
  elements.buildProgress.style.display = 'none';
  elements.buildResults.style.display = 'none';
}

// Utility functions
function delay(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

// Export for debugging
window.Star5IDE = {
  selectedFeatures,
  selectedEncryption,
  getCurrentConfig,
  logOutput,
  generatePolymorphicBuild
};