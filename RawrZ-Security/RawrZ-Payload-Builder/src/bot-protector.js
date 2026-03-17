// Bot Protection Module
const crypto = require('crypto');
const fs = require('fs');

class BotProtector {
  static encryptFile(filePath, password) {
    const data = fs.readFileSync(filePath);
    const key = crypto.scryptSync(password, 'salt', 32);
    const iv = crypto.randomBytes(16);
    const cipher = crypto.createCipher('aes-256-gcm', key);
    
    let encrypted = cipher.update(data);
    encrypted = Buffer.concat([encrypted, cipher.final()]);
    const authTag = cipher.getAuthTag();
    
    const result = Buffer.concat([iv, authTag, encrypted]);
    fs.writeFileSync(filePath + '.protected', result);
    return filePath + '.protected';
  }
  
  static obfuscateScript(scriptPath) {
    let content = fs.readFileSync(scriptPath, 'utf8');
    
    // Basic string obfuscation
    content = content.replace(/(['"])(.*?)\1/g, (match, quote, str) => {
      const encoded = Buffer.from(str).toString('base64');
      return `Buffer.from('${encoded}', 'base64').toString()`;
    });
    
    // Variable name obfuscation
    const vars = content.match(/\b[a-zA-Z_$][a-zA-Z0-9_$]*\b/g) || [];
    const uniqueVars = [...new Set(vars)];
    
    uniqueVars.forEach((varName, index) => {
      if (!['require', 'module', 'exports', 'console'].includes(varName)) {
        const obfuscated = '_0x' + index.toString(16);
        content = content.replace(new RegExp(`\\b${varName}\\b`, 'g'), obfuscated);
      }
    });
    
    fs.writeFileSync(scriptPath + '.obf', content);
    return scriptPath + '.obf';
  }
}

module.exports = BotProtector;