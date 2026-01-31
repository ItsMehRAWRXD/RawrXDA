#!/usr/bin/env node
// Quick CSP Fix for RawrZ Panel
// This script fixes Content Security Policy violations

const fs = require('fs');
const path = require('path');

console.log('🔧 Fixing CSP issues in RawrZ panel...');

// Fix the server.js CSP configuration
const serverPath = path.join(__dirname, 'server.js');
let serverContent = fs.readFileSync(serverPath, 'utf8');

// Replace the CSP configuration with a more permissive one
const newCSPConfig = `// Enhanced security headers with permissive CSP for functionality
app.use(helmet({
    contentSecurityPolicy: {
        directives: {
            defaultSrc: ["'self'", "'unsafe-inline'", "'unsafe-eval'"],
            scriptSrc: ["'self'", "'unsafe-inline'", "'unsafe-eval'", "data:", "blob:", "https:", "http:"],
            scriptSrcAttr: ["'unsafe-inline'", "'unsafe-hashes'", "'unsafe-eval'"],
            styleSrc: ["'self'", "'unsafe-inline'", "data:", "blob:", "https:", "http:"],
            styleSrcAttr: ["'unsafe-inline'", "'unsafe-hashes'"],
            imgSrc: ["'self'", "data:", "blob:", "https:", "http:"],
            fontSrc: ["'self'", "data:", "blob:", "https:", "http:"],
            connectSrc: ["'self'", "ws:", "wss:", "https:", "http:"],
            frameSrc: ["'self'", "https:", "http:"],
            frameAncestors: ["'self'", "https:", "http:"],
            objectSrc: ["'self'", "data:", "blob:"],
            mediaSrc: ["'self'", "data:", "blob:", "https:", "http:"],
            workerSrc: ["'self'", "blob:", "data:"],
            childSrc: ["'self'", "blob:", "data:"],
            formAction: ["'self'", "https:", "http:"],
            baseUri: ["'self'"],
            upgradeInsecureRequests: []
        },
        reportOnly: false
    },
    crossOriginEmbedderPolicy: false,
    crossOriginOpenerPolicy: false,
    crossOriginResourcePolicy: { policy: "cross-origin" },
    dnsPrefetchControl: false,
    frameguard: { action: 'sameorigin' },
    hidePoweredBy: true,
    hsts: { maxAge: 31536000, includeSubDomains: true, preload: true },
    ieNoOpen: true,
    noSniff: true,
    originAgentCluster: false,
    permittedCrossDomainPolicies: false,
    referrerPolicy: { policy: "strict-origin-when-cross-origin" },
    xssFilter: true
}));`;

// Replace the existing CSP configuration
serverContent = serverContent.replace(
    /\/\/ Enhanced security headers with comprehensive CSP[\s\S]*?xssFilter: true\s*\}\);/,
    newCSPConfig
);

// Also fix the advanced panel CSP
const panelCSP = `        'Content-Security-Policy': "default-src 'self' 'unsafe-inline' 'unsafe-eval' data: blob: https: http:; script-src 'self' 'unsafe-inline' 'unsafe-eval' data: blob: https: http:; script-src-attr 'unsafe-inline' 'unsafe-hashes' 'unsafe-eval'; style-src 'self' 'unsafe-inline' data: blob: https: http:; style-src-attr 'unsafe-inline' 'unsafe-hashes'; img-src 'self' data: blob: https: http:; font-src 'self' data: blob: https: http:; connect-src 'self' ws: wss: https: http:; frame-src 'self' https: http:; frame-ancestors 'self' https: http:; object-src 'self' data: blob:; media-src 'self' data: blob: https: http:; worker-src 'self' blob: data:; child-src 'self' blob: data:; form-action 'self' https: http:; base-uri 'self'; upgrade-insecure-requests;",`;

serverContent = serverContent.replace(
    /'Content-Security-Policy': "[^"]*",/,
    panelCSP
);

// Write the fixed server content
fs.writeFileSync(serverPath, serverContent);

console.log('✅ CSP configuration fixed in server.js');

// Fix favicon issue by creating a simple favicon route
const faviconFix = `
// Favicon fix
app.get('/favicon.ico', (req, res) => {
    res.status(204).end();
});

`;

// Add favicon fix if not already present
if (!serverContent.includes('app.get(\'/favicon.ico\'')) {
    const insertPoint = serverContent.indexOf('app.get(\'/health\'');
    if (insertPoint !== -1) {
        serverContent = serverContent.slice(0, insertPoint) + faviconFix + serverContent.slice(insertPoint);
        fs.writeFileSync(serverPath, serverContent);
        console.log('✅ Favicon route added');
    }
}

console.log('🎉 CSP fixes applied successfully!');
console.log('📋 Changes made:');
console.log('   - Made CSP more permissive for panel functionality');
console.log('   - Added unsafe-inline and unsafe-eval to script-src');
console.log('   - Added unsafe-hashes to script-src-attr');
console.log('   - Fixed favicon.ico 404 error');
console.log('');
console.log('🚀 Restart your server to apply the fixes:');
console.log('   node server.js');
console.log('');
console.log('🌐 Your panel should now work without CSP errors!');
