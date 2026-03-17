const fs = require('fs');
const crypto = require('crypto');

// Read the C++ source
const sourceCode = fs.readFileSync('mirc-camellia-payload.cpp');

// RAWRZ1 encryption
const key = crypto.randomBytes(32);
const iv = crypto.randomBytes(12);
const cipher = crypto.createCipheriv('aes-256-gcm', key, iv);
const encrypted = Buffer.concat([cipher.update(sourceCode), cipher.final()]);
const authTag = cipher.getAuthTag();
const header = Buffer.from('RAWRZ1');
const finalPayload = Buffer.concat([header, iv, authTag, encrypted]);

// Write encrypted file
fs.writeFileSync('mirc-camellia-encrypted.rawrz', finalPayload);

console.log('🔥 mIRC Camellia Payload Encrypted!');
console.log('📁 File: mirc-camellia-encrypted.rawrz');
console.log('📊 Original size:', sourceCode.length, 'bytes');
console.log('🔐 Encrypted size:', finalPayload.length, 'bytes');
console.log('🔑 Key:', key.toString('hex'));
console.log('💡 Ready for Jotti scan!');