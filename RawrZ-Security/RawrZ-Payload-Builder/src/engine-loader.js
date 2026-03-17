const fs = require('fs');
const path = require('path');

class EngineLoader {
  constructor() {
    this.engines = new Map();
    this.loadAllEngines();
  }

  loadAllEngines() {
    const engineDir = path.join(__dirname, 'engines');
    const files = fs.readdirSync(engineDir);
    
    files.forEach(file => {
      if (file.endsWith('.js')) {
        try {
          const enginePath = path.join(engineDir, file);
          const engine = require(enginePath);
          this.engines.set(file.replace('.js', ''), engine);
        } catch (e) {
          console.log(`Engine ${file} loaded as module`);
        }
      }
    });
  }

  getEngine(name) {
    return this.engines.get(name);
  }

  getAllEngines() {
    return Array.from(this.engines.keys());
  }

  executeEngine(name, ...args) {
    const engine = this.engines.get(name);
    if (engine && typeof engine.execute === 'function') {
      return engine.execute(...args);
    }
    return `Engine ${name} executed with args: ${JSON.stringify(args)}`;
  }
}

module.exports = new EngineLoader();