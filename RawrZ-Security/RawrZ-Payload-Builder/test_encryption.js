const fs = require('fs');
const crypto = require('crypto');

// Quick RAWRZ1 File Encryption Test
console.log('🔥 RawrZ Quick File Encryption Test');

// Create a test file first
const testContent = `// Test payload for encryption
#include <windows.h>
#include <iostream>

int main() {
    MessageBox(NULL, "Hello from RawrZ!", "Test", MB_OK);
    return 0;
}`;

console.log('📝 Creating test file...');
fs.writeFileSync('test_payload.cpp', testContent);
console.log('✅ Test file created: test_payload.cpp');

// RAWRZ1 Encryption Function
function encryptFile(inputPath, outputPath) {
  try {
    console.log(`🔐 Encrypting: ${inputPath} → ${outputPath}`);
    
    const key = crypto.randomBytes(32);
    const iv = crypto.randomBytes(12);
    const cipher = crypto.createCipheriv('aes-256-gcm', key, iv);
    const data = fs.readFileSync(inputPath);
    const enc = Buffer.concat([cipher.update(data), cipher.final()]);
    const tag = cipher.getAuthTag();
    const header = Buffer.from('RAWRZ1');
    const fileBuf = Buffer.concat([header, iv, tag, enc]);
    
    fs.writeFileSync(outputPath, fileBuf);
    
    console.log('✅ Encryption successful!');
    console.log(`🔑 Key: ${key.toString('hex')}`);
    console.log(`📊 Size: ${data.length} → ${fileBuf.length} bytes`);
    console.log(`🧪 Ready for Jotti testing: ${outputPath}`);
    
    return {
      success: true,
      key: key.toString('hex'),
      originalSize: data.length,
      encryptedSize: fileBuf.length
    };
  } catch (error) {
    console.error(`❌ Encryption failed: ${error.message}`);
    return { success: false, error: error.message };
  }
}

// Test the encryption
const result = encryptFile('test_payload.cpp', 'test_payload_encrypted.rawrz');

if (result.success) {
  console.log('');
  console.log('🏆 SUCCESS! Your RAWRZ1 encryption is working perfectly!');
  console.log('📁 Files created:');
  console.log('  - test_payload.cpp (original)');
  console.log('  - test_payload_encrypted.rawrz (encrypted)');
  console.log('');
  console.log('🎯 Next steps:');
  console.log('1. Upload test_payload_encrypted.rawrz to virusscan.jotti.org');
  console.log('2. Check for 0/X detections (like your calc success!)');
  console.log('3. Use this same encryption for your IRC/HTTP bots');
  console.log('');
  console.log('💡 Your encryption key (save this!):');
  console.log(`   ${result.key}`);
} else {
  console.log('❌ Test failed. Check your setup.');
}

// Verify files exist
console.log('');
console.log('📂 File verification:');
if (fs.existsSync('test_payload.cpp')) {
  console.log(`✅ test_payload.cpp (${fs.statSync('test_payload.cpp').size} bytes)`);
} else {
  console.log('❌ test_payload.cpp not found');
}

if (fs.existsSync('test_payload_encrypted.rawrz')) {
  console.log(`✅ test_payload_encrypted.rawrz (${fs.statSync('test_payload_encrypted.rawrz').size} bytes)`);
} else {
  console.log('❌ test_payload_encrypted.rawrz not found');
}