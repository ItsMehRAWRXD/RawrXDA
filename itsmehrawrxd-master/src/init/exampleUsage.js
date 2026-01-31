// RawrZ Safe Initialization - Example Usage
// This file demonstrates how to use the safe initialization system

const { safeStartup } = require('./safeStartup');
const { initializationValidator } = require('./initializationValidator');
const { initializationTester } = require('./testInitialization');

// Example 1: Basic safe startup
async function exampleBasicStartup() {
    console.log('Example 1: Basic Safe Startup');
    console.log('============================');
    
    try {
        // Start the system safely
        const started = await safeStartup.start();
        
        if (started) {
            console.log(' System started successfully');
            
            // Get status
            const status = safeStartup.getStatus();
            console.log(`Components initialized: ${status.components.components.length}`);
            
            // Get report
            const report = safeStartup.getReport();
            console.log(`Total components: ${report.components.summary.totalComponents}`);
            
        } else {
            console.log(' System failed to start');
        }
        
    } catch (error) {
        console.error('Error during startup:', error.message);
    }
}

// Example 2: Validation before initialization
async function exampleValidationFirst() {
    console.log('\nExample 2: Validation First');
    console.log('============================');
    
    try {
        // Validate some test data
        const testString = 'Hello, World!';
        const stringResult = initializationValidator.validateString(testString, 'example');
        
        console.log(`String validation: ${stringResult.valid ? 'PASS' : 'FAIL'}`);
        if (!stringResult.valid) {
            console.log('Errors:', stringResult.errors);
        }
        
        // Validate an object
        const testObject = { name: 'test', value: 123 };
        const objectResult = initializationValidator.validateObject(testObject, 'example');
        
        console.log(`Object validation: ${objectResult.valid ? 'PASS' : 'FAIL'}`);
        if (!objectResult.valid) {
            console.log('Errors:', objectResult.errors);
        }
        
        // Test malformed data detection
        const malformedString = 'test\x00string';
        const malformedResult = initializationValidator.validateString(malformedString, 'malformed');
        
        console.log(`Malformed data detection: ${!malformedResult.valid ? 'PASS' : 'FAIL'}`);
        if (malformedResult.valid) {
            console.log('WARNING: Malformed data was not detected!');
        }
        
    } catch (error) {
        console.error('Error during validation:', error.message);
    }
}

// Example 3: Testing the system
async function exampleTesting() {
    console.log('\nExample 3: Testing the System');
    console.log('=============================');
    
    try {
        // Run quick test
        const quickTestResult = await initializationTester.quickTest();
        console.log(`Quick test: ${quickTestResult ? 'PASS' : 'FAIL'}`);
        
        // Run full test suite
        console.log('\nRunning full test suite...');
        const fullTestResult = await initializationTester.runTests();
        console.log(`Full test suite: ${fullTestResult ? 'PASS' : 'FAIL'}`);
        
    } catch (error) {
        console.error('Error during testing:', error.message);
    }
}

// Example 4: Safe component access
async function exampleSafeComponentAccess() {
    console.log('\nExample 4: Safe Component Access');
    console.log('================================');
    
    try {
        // Start the system first
        const started = await safeStartup.start();
        
        if (started) {
            // Access components safely
            const { standardizedInitializer } = require('./standardizedInit');
            
            // Get logger component
            const logger = standardizedInitializer.getComponent('logger');
            if (logger) {
                console.log(' Logger component available');
                logger.info('Test message from example');
            } else {
                console.log(' Logger component not available');
            }
            
            // Get data integrity component
            const dataIntegrity = standardizedInitializer.getComponent('dataIntegrity');
            if (dataIntegrity) {
                console.log(' Data integrity component available');
                
                // Test UTF-8 validation
                const validation = dataIntegrity.enforceUTF8Only('test string', 'example');
                console.log(`UTF-8 validation: ${validation.valid ? 'PASS' : 'FAIL'}`);
            } else {
                console.log(' Data integrity component not available');
            }
            
            // Check component status
            const isLoggerInitialized = standardizedInitializer.isComponentInitialized('logger');
            console.log(`Logger initialized: ${isLoggerInitialized ? 'YES' : 'NO'}`);
            
        } else {
            console.log(' Cannot access components - system not started');
        }
        
    } catch (error) {
        console.error('Error accessing components:', error.message);
    }
}

// Example 5: Error handling and recovery
async function exampleErrorHandling() {
    console.log('\nExample 5: Error Handling and Recovery');
    console.log('======================================');
    
    try {
        // Test error handling
        const { safeInitializer } = require('../utils/safeInitializer');
        
        // Try to register a component after initialization
        try {
            safeInitializer.registerComponent('test', {}, { required: true });
            console.log(' Should not be able to register component after initialization');
        } catch (error) {
            console.log(' Correctly prevented component registration after initialization');
        }
        
        // Test validation error handling
        try {
            const result = initializationValidator.validateString(null, 'test');
            console.log(`Null validation handled: ${result.valid ? 'PASS' : 'FAIL'}`);
        } catch (error) {
            console.log(' Validation error not handled properly:', error.message);
        }
        
        // Test malformed data handling
        const malformedData = 'test\x00\x01\x02string';
        const result = initializationValidator.validateString(malformedData, 'malformed');
        console.log(`Malformed data handled: ${!result.valid ? 'PASS' : 'FAIL'}`);
        
    } catch (error) {
        console.error('Error during error handling test:', error.message);
    }
}

// Main function to run all examples
async function runAllExamples() {
    console.log('RawrZ Safe Initialization - Example Usage');
    console.log('==========================================');
    
    try {
        await exampleValidationFirst();
        await exampleTesting();
        await exampleErrorHandling();
        await exampleBasicStartup();
        await exampleSafeComponentAccess();
        
        console.log('\n' + '='.repeat(50));
        console.log('All examples completed successfully!');
        console.log('='.repeat(50));
        
    } catch (error) {
        console.error('Error running examples:', error.message);
    }
}

// Export functions for individual use
module.exports = {
    exampleBasicStartup,
    exampleValidationFirst,
    exampleTesting,
    exampleSafeComponentAccess,
    exampleErrorHandling,
    runAllExamples
};

// Run examples if this file is executed directly
if (require.main === module) {
    const args = process.argv.slice(2);
    
    if (args.includes('--basic')) {
        exampleBasicStartup();
    } else if (args.includes('--validation')) {
        exampleValidationFirst();
    } else if (args.includes('--testing')) {
        exampleTesting();
    } else if (args.includes('--components')) {
        exampleSafeComponentAccess();
    } else if (args.includes('--errors')) {
        exampleErrorHandling();
    } else {
        runAllExamples();
    }
}
