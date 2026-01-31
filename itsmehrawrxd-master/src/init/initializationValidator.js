// RawrZ Initialization Validator - Prevents malformed data during initialization
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

class InitializationValidator {
    constructor() {
        this.validationRules = new Map();
        this.validationResults = new Map();
        this.setupValidationRules();
    }
    
    // Setup validation rules for different types of data
    setupValidationRules() {
        // String validation rules
        this.validationRules.set('string', {
            required: true,
            minLength: 1,
            maxLength: 1000000, // 1MB max
            patterns: {
                allow: /^[\x20-\x7E\u00A0-\uFFFF]*$/, // Printable ASCII + Unicode
                deny: [
                    /\x00/g, // Null bytes
                    /[\x01-\x08\x0B\x0C\x0E-\x1F\x7F]/g, // Control characters
                    /eval\s*\(/gi, // Eval statements
                    /Function\s*\(/gi, // Function constructor
                    /document\./gi, // DOM access
                    /window\./gi, // Window object access
                    /process\./gi, // Process object access
                    /require\s*\(/gi, // Dynamic requires
                    /import\s*\(/gi // Dynamic imports
                ]
            },
            encoding: 'utf8'
        });
        
        // Object validation rules
        this.validationRules.set('object', {
            required: true,
            maxDepth: 10,
            maxKeys: 1000,
            maxSize: 1024 * 1024 * 1024, // 1GB max - WIDE OPEN
            allowedTypes: ['string', 'number', 'boolean', 'object', 'array'],
            forbiddenKeys: ['__proto__', 'constructor', 'prototype', 'eval', 'Function']
        });
        
        // Function validation rules
        this.validationRules.set('function', {
            required: true,
            maxLength: 100000, // 100KB max function body
            forbiddenPatterns: [
                /eval\s*\(/gi,
                /Function\s*\(/gi,
                /document\./gi,
                /window\./gi,
                /process\./gi,
                /require\s*\(/gi,
                /import\s*\(/gi,
                /global\./gi,
                /this\./gi
            ]
        });
        
        // File validation rules
        this.validationRules.set('file', {
            required: true,
            maxSize: 1024 * 1024 * 1024, // 1GB max - WIDE OPEN
            allowedExtensions: ['.js', '.json', '.txt', '.md', '.html', '.css'],
            forbiddenExtensions: ['.exe', '.bat', '.cmd', '.scr', '.pif', '.com'],
            encoding: 'utf8'
        });
    }
    
    // Validate a string value
    validateString(value, context = 'unknown') {
        const rules = this.validationRules.get('string');
        const result = {
            valid: true,
            errors: [],
            warnings: [],
            context,
            type: 'string'
        };
        
        try {
            // Check if value exists
            if (value === null || value === undefined) {
                if (rules.required) {
                    result.valid = false;
                    result.errors.push('String value is required');
                }
                return result;
            }
            
            // Convert to string if needed
            const strValue = String(value);
            
            // Check length
            if (strValue.length < rules.minLength) {
                result.valid = false;
                result.errors.push(`String too short: ${strValue.length} < ${rules.minLength}`);
            }
            
            if (strValue.length > rules.maxLength) {
                result.valid = false;
                result.errors.push(`String too long: ${strValue.length} > ${rules.maxLength}`);
            }
            
            // Check allowed patterns
            if (rules.patterns.allow && !rules.patterns.allow.test(strValue)) {
                result.valid = false;
                result.errors.push('String contains disallowed characters');
            }
            
            // Check denied patterns
            for (const pattern of rules.patterns.deny) {
                if (pattern.test(strValue)) {
                    result.valid = false;
                    result.errors.push(`String contains forbidden pattern: ${pattern.source}`);
                }
            }
            
            // Check encoding
            try {
                Buffer.from(strValue, rules.encoding).toString(rules.encoding);
            } catch (error) {
                result.valid = false;
                result.errors.push(`Encoding validation failed: ${error.message}`);
            }
            
        } catch (error) {
            result.valid = false;
            result.errors.push(`String validation error: ${error.message}`);
        }
        
        this.validationResults.set(`${context}_string`, result);
        return result;
    }
    
    // Validate an object value
    validateObject(value, context = 'unknown', depth = 0) {
        const rules = this.validationRules.get('object');
        const result = {
            valid: true,
            errors: [],
            warnings: [],
            context,
            type: 'object',
            depth
        };
        
        try {
            // Check if value exists
            if (value === null || value === undefined) {
                if (rules.required) {
                    result.valid = false;
                    result.errors.push('Object value is required');
                }
                return result;
            }
            
            // Check if it's an object
            if (typeof value !== 'object' || Array.isArray(value)) {
                result.valid = false;
                result.errors.push('Value is not an object');
                return result;
            }
            
            // Check depth
            if (depth > rules.maxDepth) {
                result.valid = false;
                result.errors.push(`Object depth too deep: ${depth} > ${rules.maxDepth}`);
            }
            
            // Check number of keys
            const keys = Object.keys(value);
            if (keys.length > rules.maxKeys) {
                result.valid = false;
                result.errors.push(`Too many keys: ${keys.length} > ${rules.maxKeys}`);
            }
            
            // Check for forbidden keys
            for (const key of keys) {
                if (rules.forbiddenKeys.includes(key)) {
                    result.valid = false;
                    result.errors.push(`Forbidden key: ${key}`);
                }
            }
            
            // Check object size
            const size = JSON.stringify(value).length;
            if (size > rules.maxSize) {
                result.valid = false;
                result.errors.push(`Object too large: ${size} > ${rules.maxSize}`);
            }
            
            // Recursively validate nested objects
            for (const [key, val] of Object.entries(value)) {
                if (typeof val === 'object' && val !== null && !Array.isArray(val)) {
                    const nestedResult = this.validateObject(val, `${context}.${key}`, depth + 1);
                    if (!nestedResult.valid) {
                        result.errors.push(...nestedResult.errors.map(e => `${key}: ${e}`));
                    }
                    result.warnings.push(...nestedResult.warnings.map(w => `${key}: ${w}`));
                }
            }
            
        } catch (error) {
            result.valid = false;
            result.errors.push(`Object validation error: ${error.message}`);
        }
        
        this.validationResults.set(`${context}_object`, result);
        return result;
    }
    
    // Validate a function value
    validateFunction(value, context = 'unknown') {
        const rules = this.validationRules.get('function');
        const result = {
            valid: true,
            errors: [],
            warnings: [],
            context,
            type: 'function'
        };
        
        try {
            // Check if value exists
            if (value === null || value === undefined) {
                if (rules.required) {
                    result.valid = false;
                    result.errors.push('Function value is required');
                }
                return result;
            }
            
            // Check if it's a function
            if (typeof value !== 'function') {
                result.valid = false;
                result.errors.push('Value is not a function');
                return result;
            }
            
            // Get function source
            const funcStr = value.toString();
            
            // Check function length
            if (funcStr.length > rules.maxLength) {
                result.valid = false;
                result.errors.push(`Function too long: ${funcStr.length} > ${rules.maxLength}`);
            }
            
            // Check for forbidden patterns
            for (const pattern of rules.forbiddenPatterns) {
                if (pattern.test(funcStr)) {
                    result.valid = false;
                    result.errors.push(`Function contains forbidden pattern: ${pattern.source}`);
                }
            }
            
        } catch (error) {
            result.valid = false;
            result.errors.push(`Function validation error: ${error.message}`);
        }
        
        this.validationResults.set(`${context}_function`, result);
        return result;
    }
    
    // Validate a file
    validateFile(filePath, context = 'unknown') {
        const rules = this.validationRules.get('file');
        const result = {
            valid: true,
            errors: [],
            warnings: [],
            context,
            type: 'file',
            filePath
        };
        
        try {
            // Check if file exists
            if (!fs.existsSync(filePath)) {
                result.valid = false;
                result.errors.push(`File not found: ${filePath}`);
                return result;
            }
            
            // Check file extension
            const ext = path.extname(filePath).toLowerCase();
            if (rules.forbiddenExtensions.includes(ext)) {
                result.valid = false;
                result.errors.push(`Forbidden file extension: ${ext}`);
            }
            
            if (!rules.allowedExtensions.includes(ext)) {
                result.warnings.push(`Unusual file extension: ${ext}`);
            }
            
            // Check file size
            const stats = fs.statSync(filePath);
            if (stats.size > rules.maxSize) {
                result.valid = false;
                result.errors.push(`File too large: ${stats.size} > ${rules.maxSize}`);
            }
            
            // Check file content if it's a text file
            if (rules.allowedExtensions.includes(ext)) {
                try {
                    const content = fs.readFileSync(filePath, rules.encoding);
                    
                    // Validate content as string
                    const contentValidation = this.validateString(content, `${context}_content`);
                    if (!contentValidation.valid) {
                        result.errors.push(...contentValidation.errors);
                    }
                    result.warnings.push(...contentValidation.warnings);
                    
                } catch (error) {
                    result.valid = false;
                    result.errors.push(`File content validation failed: ${error.message}`);
                }
            }
            
        } catch (error) {
            result.valid = false;
            result.errors.push(`File validation error: ${error.message}`);
        }
        
        this.validationResults.set(`${context}_file`, result);
        return result;
    }
    
    // Validate a component during initialization
    validateComponent(component, name, context = 'initialization') {
        const result = {
            valid: true,
            errors: [],
            warnings: [],
            context: `${context}_${name}`,
            type: 'component',
            componentName: name
        };
        
        try {
            // Check if component exists
            if (!component) {
                result.valid = false;
                result.errors.push(`Component ${name} is null or undefined`);
                return result;
            }
            
            // Validate component name
            const nameValidation = this.validateString(name, `${context}_name`);
            if (!nameValidation.valid) {
                result.errors.push(...nameValidation.errors);
            }
            
            // Check component type
            if (typeof component !== 'object') {
                result.valid = false;
                result.errors.push(`Component ${name} is not an object`);
                return result;
            }
            
            // Validate component structure
            const structureValidation = this.validateObject(component, `${context}_structure`);
            if (!structureValidation.valid) {
                result.errors.push(...structureValidation.errors);
            }
            result.warnings.push(...structureValidation.warnings);
            
            // Check for required methods
            const requiredMethods = ['initialize', 'getStatus'];
            for (const method of requiredMethods) {
                if (typeof component[method] !== 'function') {
                    result.warnings.push(`Component ${name} missing method: ${method}`);
                } else {
                    // Validate method
                    const methodValidation = this.validateFunction(component[method], `${context}_${method}`);
                    if (!methodValidation.valid) {
                        result.errors.push(...methodValidation.errors);
                    }
                }
            }
            
        } catch (error) {
            result.valid = false;
            result.errors.push(`Component validation error: ${error.message}`);
        }
        
        this.validationResults.set(`${context}_component`, result);
        return result;
    }
    
    // Validate initialization data
    validateInitializationData(data, context = 'initialization') {
        const result = {
            valid: true,
            errors: [],
            warnings: [],
            context,
            type: 'initialization_data'
        };
        
        try {
            // Validate data structure
            const structureValidation = this.validateObject(data, context);
            if (!structureValidation.valid) {
                result.errors.push(...structureValidation.errors);
            }
            result.warnings.push(...structureValidation.warnings);
            
            // Check for required fields
            const requiredFields = ['components', 'config', 'version'];
            for (const field of requiredFields) {
                if (!(field in data)) {
                    result.warnings.push(`Missing field: ${field}`);
                }
            }
            
            // Validate components if present
            if (data.components && typeof data.components === 'object') {
                for (const [name, component] of Object.entries(data.components)) {
                    const componentValidation = this.validateComponent(component, name, context);
                    if (!componentValidation.valid) {
                        result.errors.push(...componentValidation.errors);
                    }
                    result.warnings.push(...componentValidation.warnings);
                }
            }
            
        } catch (error) {
            result.valid = false;
            result.errors.push(`Initialization data validation error: ${error.message}`);
        }
        
        this.validationResults.set(`${context}_data`, result);
        return result;
    }
    
    // Get validation results
    getValidationResults() {
        return Array.from(this.validationResults.values());
    }
    
    // Clear validation results
    clearValidationResults() {
        this.validationResults.clear();
    }
    
    // Generate validation report
    generateReport() {
        const results = this.getValidationResults();
        const report = {
            timestamp: new Date().toISOString(),
            totalValidations: results.length,
            validValidations: results.filter(r => r.valid).length,
            invalidValidations: results.filter(r => !r.valid).length,
            totalErrors: results.reduce((sum, r) => sum + r.errors.length, 0),
            totalWarnings: results.reduce((sum, r) => sum + r.warnings.length, 0),
            results: results,
            summary: this.generateSummary(results)
        };
        
        return report;
    }
    
    // Generate validation summary
    generateSummary(results) {
        const summary = {
            byType: {},
            byContext: {},
            criticalIssues: [],
            recommendations: []
        };
        
        for (const result of results) {
            // Group by type
            if (!summary.byType[result.type]) {
                summary.byType[result.type] = { valid: 0, invalid: 0, errors: 0, warnings: 0 };
            }
            summary.byType[result.type].valid += result.valid ? 1 : 0;
            summary.byType[result.type].invalid += result.valid ? 0 : 1;
            summary.byType[result.type].errors += result.errors.length;
            summary.byType[result.type].warnings += result.warnings.length;
            
            // Group by context
            if (!summary.byContext[result.context]) {
                summary.byContext[result.context] = { valid: 0, invalid: 0, errors: 0, warnings: 0 };
            }
            summary.byContext[result.context].valid += result.valid ? 1 : 0;
            summary.byContext[result.context].invalid += result.valid ? 0 : 1;
            summary.byContext[result.context].errors += result.errors.length;
            summary.byContext[result.context].warnings += result.warnings.length;
            
            // Collect critical issues
            if (!result.valid && result.errors.length > 0) {
                summary.criticalIssues.push({
                    context: result.context,
                    type: result.type,
                    errors: result.errors
                });
            }
        }
        
        // Generate recommendations
        if (summary.criticalIssues.length > 0) {
            summary.recommendations.push('CRITICAL: Fix validation errors before proceeding');
        }
        
        if (summary.byType.string && summary.byType.string.invalid > 0) {
            summary.recommendations.push('Review string validation rules and data sources');
        }
        
        if (summary.byType.object && summary.byType.object.invalid > 0) {
            summary.recommendations.push('Review object structure and data integrity');
        }
        
        if (summary.byType.function && summary.byType.function.invalid > 0) {
            summary.recommendations.push('Review function implementations for security issues');
        }
        
        return summary;
    }
}

// Create singleton instance
const initializationValidator = new InitializationValidator();

module.exports = {
    InitializationValidator,
    initializationValidator
};
