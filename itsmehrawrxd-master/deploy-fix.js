#!/usr/bin/env node

/**
 * RawrZ Platform Deployment Fix
 * Comprehensive CSP and security configuration
 */

const express = require('express');
const helmet = require('helmet');
const path = require('path');

console.log('🚀 RawrZ Platform - Deployment Fix');
console.log('=====================================');

// Create a test server to verify CSP configuration
const app = express();

// Comprehensive CSP configuration
app.use(helmet({
    contentSecurityPolicy: {
        directives: {
            defaultSrc: ["'self'"],
            scriptSrc: ["'self'", "'unsafe-inline'", "'unsafe-eval'", "data:", "blob:", "https:"],
            scriptSrcAttr: ["'unsafe-inline'", "'unsafe-hashes'"],
            styleSrc: ["'self'", "'unsafe-inline'", "data:", "blob:", "https:"],
            styleSrcAttr: ["'unsafe-inline'"],
            imgSrc: ["'self'", "data:", "blob:", "https:", "http:"],
            fontSrc: ["'self'", "data:", "blob:", "https:"],
            connectSrc: ["'self'", "ws:", "wss:", "https:", "http:"],
            frameAncestors: ["'none'"],
            baseUri: ["'self'"],
            formAction: ["'self'"],
            objectSrc: ["'none'"],
            mediaSrc: ["'self'", "data:", "blob:", "https:"],
            workerSrc: ["'self'", "blob:"],
            manifestSrc: ["'self'"],
            childSrc: ["'self'", "blob:"],
            frameSrc: ["'none'"],
            upgradeInsecureRequests: []
        },
        reportOnly: false
    }
}));

// CSP override middleware
app.use((req, res, next) => {
    res.setHeader('Content-Security-Policy', 
        "default-src 'self'; " +
        "script-src 'self' 'unsafe-inline' 'unsafe-eval' data: blob: https:; " +
        "script-src-attr 'unsafe-inline' 'unsafe-hashes'; " +
        "style-src 'self' 'unsafe-inline' data: blob: https:; " +
        "style-src-attr 'unsafe-inline'; " +
        "img-src 'self' data: blob: https: http:; " +
        "font-src 'self' data: blob: https:; " +
        "connect-src 'self' ws: wss: https: http:; " +
        "frame-ancestors 'none'; " +
        "base-uri 'self'; " +
        "form-action 'self'; " +
        "object-src 'none'; " +
        "media-src 'self' data: blob: https:; " +
        "worker-src 'self' blob:; " +
        "manifest-src 'self'; " +
        "child-src 'self' blob:; " +
        "frame-src 'none';"
    );
    
    // Additional security headers
    res.setHeader('X-Content-Type-Options', 'nosniff');
    res.setHeader('X-Frame-Options', 'DENY');
    res.setHeader('X-XSS-Protection', '1; mode=block');
    res.setHeader('Referrer-Policy', 'strict-origin-when-cross-origin');
    
    next();
});

// Test routes
app.get('/test-csp', (req, res) => {
    res.send(`
        <!DOCTYPE html>
        <html>
        <head>
            <title>CSP Test</title>
        </head>
        <body>
            <h1>CSP Test Page</h1>
            <button onclick="testFunction()">Test Inline Handler</button>
            <script>
                function testFunction() {
                    alert('CSP Test Successful!');
                }
            </script>
        </body>
        </html>
    `);
});

app.get('/favicon.ico', (req, res) => res.status(204).end());

const PORT = process.env.PORT || 3001;
app.listen(PORT, () => {
    console.log(`✅ Test server running on port ${PORT}`);
    console.log(`🔗 Test CSP: http://localhost:${PORT}/test-csp`);
    console.log('📋 CSP Configuration Applied Successfully');
    console.log('🎯 All inline scripts and event handlers should work');
});

module.exports = app;
