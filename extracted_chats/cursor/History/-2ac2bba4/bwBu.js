const path = require('path');
const fs = require('fs');

/**
 * Agentic Asset Resolver for MyCoPilot++ IDE
 * Handles asset resolution across pkg snapshots and runtime environments
 */
function resolveAsset(filename, fallbackDirs = []) {
  const candidates = [
    path.join(__dirname, '..', filename),
    path.join(process.cwd(), filename),
    ...fallbackDirs.map(dir => path.join(dir, filename))
  ];

  for (const p of candidates) {
    if (fs.existsSync(p)) {
      console.log(`[HUD] Resolved asset: ${filename} → ${p}`);
      return p;
    }
  }

  console.warn(`[Audit] Asset not found: ${filename}`);
  return null;
}

/**
 * Enhanced asset resolver with HUD overlay support
 */
function resolveAssetWithHUD(filename, options = {}) {
  const {
    fallbackDirs = [],
    logLevel = 'info',
    enableHUD = true
  } = options;

  const assetPath = resolveAsset(filename, fallbackDirs);

  if (enableHUD) {
    if (assetPath) {
      console.log(`[HUD] ✅ Asset resolved: ${filename}`);
      console.log(`[HUD] 📁 Path: ${assetPath}`);
      console.log(`[HUD] 🔍 Resolution method: ${assetPath.includes('snapshot') ? 'snapshot' : 'filesystem'}`);
    } else {
      console.warn(`[HUD] ❌ Asset not found: ${filename}`);
      console.warn(`[HUD] 🔍 Searched paths:`, [
        path.join(__dirname, '..', filename),
        path.join(process.cwd(), filename),
        ...fallbackDirs.map(dir => path.join(dir, filename))
      ]);
    }
  }

  return assetPath;
}

/**
 * Express middleware for serving assets with fallback
 */
function createAssetMiddleware(options = {}) {
  return (req, res, next) => {
    const { assetPath, fallbackMessage = 'Asset not found' } = options;

    if (req.path === '/' || req.path === '') {
      const resolvedPath = resolveAssetWithHUD(assetPath || 'index.html', options);

      if (resolvedPath) {
        res.sendFile(resolvedPath);
      } else {
        res.status(404).send(fallbackMessage);
      }
    } else {
      next();
    }
  };
}

module.exports = {
  resolveAsset,
  resolveAssetWithHUD,
  createAssetMiddleware
};
