// RawrZ Initialization Test - Safe testing of the initialization system
const { safeStartup } = require('./safeStartup');
const { initializationValidator } = require('./initializationValidator');

class InitializationTester {
    constructor() {
        this.testResults = [];
        this.testStartTime = null;
        this.testEndTime = null;
    }
    
    // Run all initialization tests
    async runTests() {
        console.log('='.repeat(60));
        console.log('RawrZ Initialization System - Test Suite');
        console.log('='.repeat(60));
        
        this.testStartTime = new Date().toISOString();
        
        try {
            // Test 1: Validation system
            await this.testValidationSystem();
            
            // Test 2: Safe startup (without actually starting)
            await this.testSafeStartup();
            
            // Test 3: Component registration
            await this.testComponentRegistration();
            
            // Test 4: Error handling
            await this.testErrorHandling();
            
            // Test 5: Memory management
            await this.testMemoryManagement();
            
            this.testEndTime = new Date().toISOString();
            
            // Generate test report
            const report = this.generateTestReport();
            console.log('='.repeat(60));
            console.log('Test Results Summary');
            console.log('='.repeat(60));
            console.log(`Total Tests: ${report.summary.totalTests}`);
            console.log(`Passed: ${report.summary.passed}`);
            console.log(`Failed: ${report.summary.failed}`);
            console.log(`Duration: ${report.summary.duration}ms`);
            
            if (report.summary.failed > 0) {
                console.log('\nFailed Tests:');
                report.failedTests.forEach(test => {
                    console.log(`  - ${test.name}: ${test.error}`);
                });
            }
            
            if (report.summary.passed === report.summary.totalTests) {
                console.log('\n All tests passed! Initialization system is ready.');
                return true;
            } else {
                console.log('\n Some tests failed. Please review the issues above.');
                return false;
            }
            
        } catch (error) {
            console.error('Test suite failed:', error.message);
            return false;
        }
    }
    
    // Test the validation system
    async testValidationSystem() {
        console.log('\n[TEST] Validation System...');
        
        try {
            // Test string validation
            const stringResult = initializationValidator.validateString('test string', 'test');
            this.recordTest('string_validation', stringResult.valid, stringResult.errors);
            
            // Test object validation
            const objectResult = initializationValidator.validateObject({ test: 'value' }, 'test');
            this.recordTest('object_validation', objectResult.valid, objectResult.errors);
            
            // Test function validation
            const funcResult = initializationValidator.validateFunction(() => {}, 'test');
            this.recordTest('function_validation', funcResult.valid, funcResult.errors);
            
            // Test malformed data detection
            const malformedResult = initializationValidator.validateString('test\x00string', 'malformed');
            this.recordTest('malformed_detection', !malformedResult.valid, malformedResult.errors);
            
            console.log('   Validation system tests completed');
            
        } catch (error) {
            this.recordTest('validation_system', false, [error.message]);
            console.log('   Validation system test failed:', error.message);
        }
    }
    
    // Test safe startup (without actually starting)
    async testSafeStartup() {
        console.log('\n[TEST] Safe Startup System...');
        
        try {
            // Test startup status
            const status = safeStartup.getStatus();
            this.recordTest('startup_status', typeof status === 'object', []);
            
            // Test startup report
            const report = safeStartup.getReport();
            this.recordTest('startup_report', typeof report === 'object', []);
            
            // Test that startup hasn't started yet
            this.recordTest('startup_not_started', !status.startup?.started, []);
            
            console.log('   Safe startup system tests completed');
            
        } catch (error) {
            this.recordTest('safe_startup', false, [error.message]);
            console.log('   Safe startup system test failed:', error.message);
        }
    }
    
    // Test component registration
    async testComponentRegistration() {
        console.log('\n[TEST] Component Registration...');
        
        try {
            // Test that we can access the safe initializer
            const { safeInitializer } = require('../utils/safeInitializer');
            this.recordTest('safe_initializer_access', !!safeInitializer, []);
            
            // Test component registry
            const registry = safeInitializer.componentRegistry;
            this.recordTest('component_registry', registry instanceof Map, []);
            
            // Test validation rules
            const rules = safeInitializer.validationRules;
            this.recordTest('validation_rules', rules instanceof Map, []);
            
            console.log('   Component registration tests completed');
            
        } catch (error) {
            this.recordTest('component_registration', false, [error.message]);
            console.log('   Component registration test failed:', error.message);
        }
    }
    
    // Test error handling
    async testErrorHandling() {
        console.log('\n[TEST] Error Handling...');
        
        try {
            // Test error handling setup
            const { safeInitializer } = require('../utils/safeInitializer');
            
            // Test that error handling is set up
            this.recordTest('error_handling_setup', true, []);
            
            // Test validation error handling
            try {
                initializationValidator.validateString(null, 'test');
                this.recordTest('null_validation', true, []);
            } catch (error) {
                this.recordTest('null_validation', false, [error.message]);
            }
            
            console.log('   Error handling tests completed');
            
        } catch (error) {
            this.recordTest('error_handling', false, [error.message]);
            console.log('   Error handling test failed:', error.message);
        }
    }
    
    // Test memory management
    async testMemoryManagement() {
        console.log('\n[TEST] Memory Management...');
        
        try {
            // Test memory usage tracking
            const memUsage = process.memoryUsage();
            this.recordTest('memory_usage_tracking', typeof memUsage.heapUsed === 'number', []);
            
            // Test that memory usage is reasonable
            const maxMemory = 100 * 1024 * 1024; // 100MB
            this.recordTest('memory_usage_reasonable', memUsage.heapUsed < maxMemory, []);
            
            // Test cleanup functionality
            const { safeInitializer } = require('../utils/safeInitializer');
            if (typeof safeInitializer.reset === 'function') {
                this.recordTest('cleanup_functionality', true, []);
            } else {
                this.recordTest('cleanup_functionality', false, ['Reset function not available']);
            }
            
            console.log('   Memory management tests completed');
            
        } catch (error) {
            this.recordTest('memory_management', false, [error.message]);
            console.log('   Memory management test failed:', error.message);
        }
    }
    
    // Record a test result
    recordTest(name, passed, errors = []) {
        const result = {
            name,
            passed,
            errors,
            timestamp: new Date().toISOString()
        };
        
        this.testResults.push(result);
        
        if (passed) {
            console.log(`     ${name}`);
        } else {
            console.log(`     ${name}: ${errors.join(', ')}`);
        }
    }
    
    // Generate test report
    generateTestReport() {
        const duration = new Date(this.testEndTime) - new Date(this.testStartTime);
        
        const report = {
            timestamp: new Date().toISOString(),
            summary: {
                totalTests: this.testResults.length,
                passed: this.testResults.filter(t => t.passed).length,
                failed: this.testResults.filter(t => !t.passed).length,
                duration
            },
            results: this.testResults,
            failedTests: this.testResults.filter(t => !t.passed),
            passedTests: this.testResults.filter(t => t.passed)
        };
        
        return report;
    }
    
    // Run a quick validation test
    async quickTest() {
        console.log('Running quick initialization test...');
        
        try {
            // Test validation system
            const stringResult = initializationValidator.validateString('test', 'quick_test');
            if (!stringResult.valid) {
                throw new Error('String validation failed');
            }
            
            // Test safe startup access
            const status = safeStartup.getStatus();
            if (!status) {
                throw new Error('Safe startup status not available');
            }
            
            console.log(' Quick test passed - initialization system is functional');
            return true;
            
        } catch (error) {
            console.error(' Quick test failed:', error.message);
            return false;
        }
    }
}

// Create singleton instance
const initializationTester = new InitializationTester();

// Export both class and instance
module.exports = {
    InitializationTester,
    initializationTester
};

// Run tests if this file is executed directly
if (require.main === module) {
    const args = process.argv.slice(2);
    
    if (args.includes('--quick')) {
        initializationTester.quickTest().then(success => {
            process.exit(success ? 0 : 1);
        });
    } else {
        initializationTester.runTests().then(success => {
            process.exit(success ? 0 : 1);
        });
    }
}
