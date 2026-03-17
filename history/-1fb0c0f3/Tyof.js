const httpProxy = require('http-proxy');
const https = require('https');
const fs = require('fs');
const path = require('path');
const forge = require('node-forge');

// Create a self-signed certificate for MITM
const pki = forge.pki;
const keys = pki.rsa.generateKeyPair(2048);
const cert = pki.createCertificate();
cert.publicKey = keys.publicKey;
cert.serialNumber = '01';
cert.validity.notBefore = new Date();
cert.validity.notAfter = new Date();
cert.validity.notAfter.setFullYear(cert.validity.notBefore.getFullYear() + 1);
const attrs = [{
  name: 'commonName',
  value: 'localhost'
}, {
  name: 'countryName',
  value: 'US'
}, {
  shortName: 'ST',
  value: 'Test'
}, {
  name: 'localityName',
  value: 'Test'
}, {
  name: 'organizationName',
  value: 'Test'
}, {
  shortName: 'OU',
  value: 'Test'
}];
cert.setSubject(attrs);
cert.setIssuer(attrs);
cert.sign(keys.privateKey);

const pem = {
  key: forge.pki.privateKeyToPem(keys.privateKey),
  cert: forge.pki.certificateToPem(cert)
};

// Save certs to files
fs.writeFileSync(path.join(__dirname, 'key.pem'), pem.key);
fs.writeFileSync(path.join(__dirname, 'cert.pem'), pem.cert);

// Create proxy server
const proxy = httpProxy.createProxyServer({
  target: 'https://view.awsapps.com',
  changeOrigin: true,
  secure: false
});

// Intercept and redirect specific requests
proxy.on('proxyReq', (proxyReq, req, res, options) => {
  if (req.headers.host === 'view.awsapps.com') {
    console.log('Intercepting AmazonQ request:', req.url);
    // Redirect to mock server
    proxyReq.setHeader('host', 'localhost:3000');
    options.target = 'http://localhost:3000';
  }
});

// HTTPS server for MITM
const server = https.createServer({
  key: pem.key,
  cert: pem.cert
}, (req, res) => {
  proxy.web(req, res);
});

server.listen(8443, () => {
  console.log('Custom Fiddler-like proxy running on https://localhost:8443');
  console.log('Configure VS Code proxy to http://localhost:8443');
  console.log('Make sure mock-amazonq-server.js is running on port 3000');
});