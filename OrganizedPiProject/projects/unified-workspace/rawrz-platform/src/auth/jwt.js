// src/auth/jwt.js
'use strict';

const jwt = require('jsonwebtoken');
const crypto = require('crypto');
const { eventBus, COMMON_EVENTS } = require('../event_bus');

class JWTAuth {
  constructor(options = {}) {
    this.secret = options.secret || process.env.JWT_SECRET || this.generateSecret();
    this.refreshSecret = options.refreshSecret || process.env.JWT_REFRESH_SECRET || this.generateSecret();
    this.accessTokenExpiry = options.accessTokenExpiry || '15m';
    this.refreshTokenExpiry = options.refreshTokenExpiry || '7d';
    this.issuer = options.issuer || 'rawrz-platform';
    this.audience = options.audience || 'rawrz-client';

    // Role-based permissions
    this.permissions = {
      'admin': ['*'], // Admin has all permissions
      'crypto_user': ['crypto.*', 'files.read', 'files.upload'],
      'system_user': ['sysinfo.*', 'workflow.read', 'workflow.execute'],
      'readonly_user': ['sysinfo.read', 'files.read', 'workflow.read'],
      'guest': ['health', 'auth.login']
    };
  }

  generateSecret() {
    return crypto.randomBytes(64).toString('hex');
  }

  // Generate access token
  generateAccessToken(payload) {
    const tokenPayload = {
      ...payload,
      type: 'access',
      iat: Math.floor(Date.now() / 1000),
      exp: Math.floor(Date.now() / 1000) + this.parseExpiry(this.accessTokenExpiry),
      iss: this.issuer,
      aud: this.audience
    };

    return jwt.sign(tokenPayload, this.secret);
  }

  // Generate refresh token
  generateRefreshToken(payload) {
    const tokenPayload = {
      ...payload,
      type: 'refresh',
      iat: Math.floor(Date.now() / 1000),
      exp: Math.floor(Date.now() / 1000) + this.parseExpiry(this.refreshTokenExpiry),
      iss: this.issuer,
      aud: this.audience
    };

    return jwt.sign(tokenPayload, this.refreshSecret);
  }

  // Parse expiry string (e.g., '15m', '7d')
  parseExpiry(expiry) {
    const match = expiry.match(/^(\d+)([smhd])$/);
    if (!match) return 900; // Default 15 minutes

    const value = parseInt(match[1]);
    const unit = match[2];

    switch (unit) {
      case 's': return value;
      case 'm': return value * 60;
      case 'h': return value * 60 * 60;
      case 'd': return value * 60 * 60 * 24;
      default: return 900;
    }
  }

  // Verify access token
  verifyAccessToken(token) {
    try {
      const decoded = jwt.verify(token, this.secret);
      if (decoded.type !== 'access') {
        throw new Error('Invalid token type');
      }
      return decoded;
    } catch (error) {
      eventBus.emit(COMMON_EVENTS.AUTH_FAILED, {
        reason: 'invalid_access_token',
        error: error.message
      });
      throw error;
    }
  }

  // Verify refresh token
  verifyRefreshToken(token) {
    try {
      const decoded = jwt.verify(token, this.refreshSecret);
      if (decoded.type !== 'refresh') {
        throw new Error('Invalid token type');
      }
      return decoded;
    } catch (error) {
      eventBus.emit(COMMON_EVENTS.AUTH_FAILED, {
        reason: 'invalid_refresh_token',
        error: error.message
      });
      throw error;
    }
  }

  // Check if user has permission
  hasPermission(userRoles, requiredPermission) {
    if (!userRoles || !Array.isArray(userRoles)) {
      return false;
    }

    for (const role of userRoles) {
      const rolePermissions = this.permissions[role] || [];
      
      // Check for wildcard permission
      if (rolePermissions.includes('*')) {
        return true;
      }

      // Check for exact permission match
      if (rolePermissions.includes(requiredPermission)) {
        return true;
      }

      // Check for wildcard permission (e.g., 'crypto.*' matches 'crypto.hash')
      for (const permission of rolePermissions) {
        if (permission.endsWith('*')) {
          const prefix = permission.slice(0, -1);
          if (requiredPermission.startsWith(prefix)) {
            return true;
          }
        }
      }
    }

    return false;
  }

  // Generate token pair
  generateTokenPair(user) {
    const payload = {
      userId: user.id,
      username: user.username,
      roles: user.roles || ['guest'],
      email: user.email
    };

    const accessToken = this.generateAccessToken(payload);
    const refreshToken = this.generateRefreshToken(payload);

    eventBus.emit(COMMON_EVENTS.AUTH_LOGIN, {
      userId: user.id,
      username: user.username,
      roles: user.roles
    });

    return {
      accessToken,
      refreshToken,
      expiresIn: this.parseExpiry(this.accessTokenExpiry),
      tokenType: 'Bearer'
    };
  }

  // Refresh access token
  refreshAccessToken(refreshToken) {
    const decoded = this.verifyRefreshToken(refreshToken);
    
    const newPayload = {
      userId: decoded.userId,
      username: decoded.username,
      roles: decoded.roles,
      email: decoded.email
    };

    const newAccessToken = this.generateAccessToken(newPayload);
    const newRefreshToken = this.generateRefreshToken(newPayload);

    eventBus.emit(COMMON_EVENTS.AUTH_TOKEN_REFRESH, {
      userId: decoded.userId,
      username: decoded.username
    });

    return {
      accessToken: newAccessToken,
      refreshToken: newRefreshToken,
      expiresIn: this.parseExpiry(this.accessTokenExpiry),
      tokenType: 'Bearer'
    };
  }

  // Extract token from request
  extractTokenFromRequest(req) {
    const authHeader = req.headers.authorization;
    if (!authHeader || !authHeader.startsWith('Bearer ')) {
      return null;
    }
    return authHeader.slice(7).trim();
  }

  // Middleware for authentication
  authenticate() {
    return (req, res, next) => {
      const token = this.extractTokenFromRequest(req);
      
      if (!token) {
        return res.status(401).json({ error: 'No token provided' });
      }

      try {
        const decoded = this.verifyAccessToken(token);
        req.user = decoded;
        next();
      } catch (error) {
        return res.status(401).json({ error: 'Invalid token' });
      }
    };
  }

  // Middleware for authorization
  authorize(permission) {
    return (req, res, next) => {
      if (!req.user) {
        return res.status(401).json({ error: 'Authentication required' });
      }

      if (!this.hasPermission(req.user.roles, permission)) {
        eventBus.emit(COMMON_EVENTS.SECURITY_VIOLATION, {
          userId: req.user.userId,
          username: req.user.username,
          attemptedPermission: permission,
          userRoles: req.user.roles
        });

        return res.status(403).json({ 
          error: 'Insufficient permissions',
          required: permission,
          userRoles: req.user.roles
        });
      }

      next();
    };
  }

  // Logout (invalidate refresh token)
  logout(refreshToken) {
    try {
      const decoded = this.verifyRefreshToken(refreshToken);
      
      eventBus.emit(COMMON_EVENTS.AUTH_LOGOUT, {
        userId: decoded.userId,
        username: decoded.username
      });

      return true;
    } catch (error) {
      return false;
    }
  }

  // Get user info from token
  getUserInfo(token) {
    try {
      const decoded = this.verifyAccessToken(token);
      return {
        id: decoded.userId,
        username: decoded.username,
        email: decoded.email,
        roles: decoded.roles,
        permissions: this.getUserPermissions(decoded.roles)
      };
    } catch (error) {
      throw new Error('Invalid token');
    }
  }

  // Get user permissions based on roles
  getUserPermissions(roles) {
    const permissions = new Set();
    
    for (const role of roles) {
      const rolePermissions = this.permissions[role] || [];
      rolePermissions.forEach(permission => {
        if (permission === '*') {
          // Add all possible permissions
          Object.values(this.permissions).flat().forEach(p => {
            if (p !== '*') permissions.add(p);
          });
        } else {
          permissions.add(permission);
        }
      });
    }

    return Array.from(permissions);
  }
}

module.exports = JWTAuth;
