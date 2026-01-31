const { v4: uuidv4, v5: uuidv5 } = require('uuid');
const crypto = require('crypto');
const fs = require('fs').promises;
const path = require('path');

// UUID generation utilities
const generateUUID = () => uuidv4();
const generateNamedUUID = (name) => uuidv5(name, uuidv5.DNS);

// File handling utilities
const generateUniqueFilename = (originalName, prefix = '') => {
  const ext = path.extname(originalName);
  const name = path.basename(originalName, ext);
  const timestamp = Date.now();
  const uuid = generateUUID().substring(0, 8);
  return `${prefix}${name}_${timestamp}_${uuid}${ext}`;
};

// Secure file operations
const secureFileWrite = async (filePath, data, options = {}) => {
  try {
    // Ensure directory exists
    const dir = path.dirname(filePath);
    await fs.mkdir(dir, { recursive: true });
    
    // Write file with proper permissions
    await fs.writeFile(filePath, data, { 
      mode: 0o600, // Read/write for owner only
      ...options 
    });
    
    return { success: true, path: filePath };
  } catch (error) {
    return { success: false, error: error.message };
  }
};

const secureFileRead = async (filePath) => {
  try {
    const data = await fs.readFile(filePath);
    return { success: true, data };
  } catch (error) {
    return { success: false, error: error.message };
  }
};

// Crypto utilities
const generateSecureKey = (length = 32) => {
  return crypto.randomBytes(length).toString('hex');
};

const hashData = (data, algorithm = 'sha256') => {
  return crypto.createHash(algorithm).update(data).digest('hex');
};

const encryptData = (data, key) => {
  const iv = crypto.randomBytes(16);
  // Handle both hex and base64 keys
  let keyBuffer;
  if (Buffer.isBuffer(key)) {
    keyBuffer = key;
  } else if (key.length === 64) { // hex key (32 bytes = 64 hex chars)
    keyBuffer = Buffer.from(key, 'hex');
  } else { // base64 key
    keyBuffer = Buffer.from(key, 'base64');
  }
  const cipher = crypto.createCipheriv('aes-256-cbc', keyBuffer, iv);
  let encrypted = cipher.update(data, 'utf8', 'hex');
  encrypted += cipher.final('hex');
  return { encrypted, iv: iv.toString('hex') };
};

const decryptData = (encryptedData, key, iv) => {
  const keyBuffer = Buffer.isBuffer(key) ? key : Buffer.from(key, 'hex');
  const decipher = crypto.createDecipheriv('aes-256-cbc', keyBuffer, Buffer.from(iv, 'hex'));
  let decrypted = decipher.update(encryptedData, 'hex', 'utf8');
  decrypted += decipher.final('utf8');
  return decrypted;
};

// Session management
const generateSessionId = () => {
  return generateUUID();
};

const generateApiKey = () => {
  return `rawrz_${generateSecureKey(24)}`;
};

// File validation
const validateFileType = (filename, allowedTypes = []) => {
  const ext = path.extname(filename).toLowerCase();
  return allowedTypes.length === 0 || allowedTypes.includes(ext);
};

const validateFileSize = (size, maxSize = 1024 * 1024 * 1024) => { // 1GB default - WIDE OPEN
  return size <= maxSize;
};

// Response formatting
const formatResponse = (success, data = null, message = '', code = 200) => {
  return {
    success,
    code,
    message,
    data,
    timestamp: new Date().toISOString(),
    requestId: generateUUID()
  };
};

// Error handling
const handleError = (error, context = '') => {
  const errorId = generateUUID();
  return {
    error: true,
    errorId,
    message: error.message,
    context,
    timestamp: new Date().toISOString(),
    stack: process.env.NODE_ENV === 'development' ? error.stack : undefined
  };
};

// Rate limiting helpers
const createRateLimitKey = (req) => {
  return `${req.ip}_${req.user?.id || 'anonymous'}`;
};

// Security helpers
const sanitizeInput = (input) => {
  if (typeof input !== 'string') return input;
  return input
    .replace(/[<>]/g, '') // Remove potential HTML tags
    .replace(/javascript:/gi, '') // Remove javascript: protocol
    .trim();
};

const generateNonce = () => {
  return crypto.randomBytes(16).toString('base64');
};

module.exports = {
  // UUID utilities
  generateUUID,
  generateNamedUUID,
  
  // File utilities
  generateUniqueFilename,
  secureFileWrite,
  secureFileRead,
  validateFileType,
  validateFileSize,
  
  // Crypto utilities
  generateSecureKey,
  hashData,
  encryptData,
  decryptData,
  
  // Session utilities
  generateSessionId,
  generateApiKey,
  
  // Response utilities
  formatResponse,
  handleError,
  
  // Security utilities
  createRateLimitKey,
  sanitizeInput,
  generateNonce
};
