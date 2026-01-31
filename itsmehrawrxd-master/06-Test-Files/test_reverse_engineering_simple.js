#!/usr/bin/env node

// Simple test script for RawrZ Reverse Engineering Engine
const ReverseEngineering = require('./src/engines/reverse-engineering');

async function testReverseEngineering() {
    console.log(' Testing RawrZ Reverse Engineering Engine...\n');
    
    try {
        // Initialize the engine
        console.log('1. Initializing Reverse Engineering Engine...');
        const reEngine = ReverseEngineering;
        await reEngine.initialize();
        console.log(' Reverse Engineering Engine initialized successfully\n');
        
        // Test binary analysis
        console.log('2. Testing binary analysis...');
        const testFile = 'test_sample.exe';
        const analysisResult = await reEngine.analyzeBinary(testFile, {
            analyzeSections: true,
            analyzeImports: true,
            analyzeExports: true,
            extractStrings: true,
            analyzeFunctions: true
        });
        console.log(' Binary analysis completed:', analysisResult);
        console.log('');
        
        // Test disassembly
        console.log('3. Testing disassembly functionality...');
        const disassemblyResult = await reEngine.disassemble(testFile, {
            architecture: 'x64',
            format: 'PE',
            startAddress: 0x401000,
            endAddress: 0x401100
        });
        console.log(' Disassembly completed:', disassemblyResult);
        console.log('');
        
        // Test string extraction
        console.log('4. Testing string extraction...');
        const stringResult = await reEngine.extractStrings(testFile, {
            minLength: 4,
            encoding: 'utf8',
            includeUnicode: true
        });
        console.log(' String extraction completed:', stringResult);
        console.log('');
        
        // Test function analysis
        console.log('5. Testing function analysis...');
        const functionResult = await reEngine.analyzeFunctions(testFile, {
            includeImports: true,
            includeExports: true,
            analyzeCallGraph: true
        });
        console.log(' Function analysis completed:', functionResult);
        console.log('');
        
        // Test packing detection
        console.log('6. Testing packing detection...');
        const packingResult = await reEngine.detectPacking(testFile);
        console.log(' Packing detection completed:', packingResult);
        console.log('');
        
        // Test obfuscation detection
        console.log('7. Testing obfuscation detection...');
        const obfuscationResult = await reEngine.detectObfuscation(testFile);
        console.log(' Obfuscation detection completed:', obfuscationResult);
        console.log('');
        
        // Test malware analysis
        console.log('8. Testing malware analysis...');
        const malwareResult = await reEngine.analyzeMalware(testFile);
        console.log(' Malware analysis completed:', malwareResult);
        console.log('');
        
        // Test entropy analysis
        console.log('9. Testing entropy analysis...');
        const entropyResult = await reEngine.analyzeEntropy(testFile);
        console.log(' Entropy analysis completed:', entropyResult);
        console.log('');
        
        // Test section analysis
        console.log('10. Testing section analysis...');
        const sectionResult = await reEngine.analyzeSections(testFile);
        console.log(' Section analysis completed:', sectionResult);
        console.log('');
        
        // Test import analysis
        console.log('11. Testing import analysis...');
        const importResult = await reEngine.analyzeImports(testFile);
        console.log(' Import analysis completed:', importResult);
        console.log('');
        
        // Test export analysis
        console.log('12. Testing export analysis...');
        const exportResult = await reEngine.analyzeExports(testFile);
        console.log(' Export analysis completed:', exportResult);
        console.log('');
        
        // Test decompilation
        console.log('13. Testing decompilation...');
        const decompileResult = await reEngine.decompile(testFile, {
            functionAddress: 0x401000,
            architecture: 'x64'
        });
        console.log(' Decompilation completed:', decompileResult);
        console.log('');
        
        // Test control flow analysis
        console.log('14. Testing control flow analysis...');
        const cfResult = await reEngine.analyzeControlFlow(testFile, {
            startAddress: 0x401000,
            maxDepth: 10
        });
        console.log(' Control flow analysis completed:', cfResult);
        console.log('');
        
        // Test data flow analysis
        console.log('15. Testing data flow analysis...');
        const dfResult = await reEngine.analyzeDataFlow(testFile, {
            startAddress: 0x401000,
            trackRegisters: true
        });
        console.log(' Data flow analysis completed:', dfResult);
        console.log('');
        
        // Test cross-reference analysis
        console.log('16. Testing cross-reference analysis...');
        const xrefResult = await reEngine.analyzeCrossReferences(testFile, {
            targetAddress: 0x401000,
            includeData: true
        });
        console.log(' Cross-reference analysis completed:', xrefResult);
        console.log('');
        
        // Test pattern matching
        console.log('17. Testing pattern matching...');
        const patternResult = await reEngine.matchPatterns(testFile, {
            patterns: [
                { name: 'API_CALL', bytes: 'FF 15 ?? ?? ?? ??' },
                { name: 'JMP_INDIRECT', bytes: 'FF 25 ?? ?? ?? ??' },
                { name: 'CALL_INDIRECT', bytes: 'FF 55 ??' }
            ]
        });
        console.log(' Pattern matching completed:', patternResult);
        console.log('');
        
        // Test YARA rules
        console.log('18. Testing YARA rule matching...');
        const yaraResult = await reEngine.matchYaraRules(testFile, {
            rules: [
                'rule test_rule { strings: $a = "test" condition: $a }'
            ]
        });
        console.log(' YARA rule matching completed:', yaraResult);
        console.log('');
        
        // Test code similarity
        console.log('19. Testing code similarity analysis...');
        const similarityResult = await reEngine.analyzeCodeSimilarity(testFile, {
            referenceFile: 'reference.exe',
            algorithm: 'ssdeep'
        });
        console.log(' Code similarity analysis completed:', similarityResult);
        console.log('');
        
        // Test vulnerability detection
        console.log('20. Testing vulnerability detection...');
        const vulnResult = await reEngine.detectVulnerabilities(testFile, {
            checkTypes: ['buffer_overflow', 'format_string', 'integer_overflow', 'use_after_free']
        });
        console.log(' Vulnerability detection completed:', vulnResult);
        console.log('');
        
        // Test anti-analysis detection
        console.log('21. Testing anti-analysis detection...');
        const antiAnalysisResult = await reEngine.detectAntiAnalysis(testFile);
        console.log(' Anti-analysis detection completed:', antiAnalysisResult);
        console.log('');
        
        // Test emulation
        console.log('22. Testing emulation...');
        const emulationResult = await reEngine.emulate(testFile, {
            startAddress: 0x401000,
            maxInstructions: 1000,
            traceExecution: true
        });
        console.log(' Emulation completed:', emulationResult);
        console.log('');
        
        // Test dynamic analysis
        console.log('23. Testing dynamic analysis...');
        const dynamicResult = await reEngine.performDynamicAnalysis(testFile, {
            monitorApiCalls: true,
            monitorFileAccess: true,
            monitorNetworkActivity: true
        });
        console.log(' Dynamic analysis completed:', dynamicResult);
        console.log('');
        
        // Test report generation
        console.log('24. Testing report generation...');
        const reportResult = await reEngine.generateReport(testFile, {
            format: 'json',
            includeAllAnalyses: true
        });
        console.log(' Report generation completed:', reportResult);
        console.log('');
        
        // Test batch analysis
        console.log('25. Testing batch analysis...');
        const batchResult = await reEngine.analyzeBatch(['test1.exe', 'test2.exe', 'test3.exe'], {
            parallel: true,
            maxConcurrent: 3
        });
        console.log(' Batch analysis completed:', batchResult);
        console.log('');
        
        // Test plugin system
        console.log('26. Testing plugin system...');
        const pluginResult = await reEngine.loadPlugin('custom_analyzer', {
            pluginPath: './plugins/custom_analyzer.js'
        });
        console.log(' Plugin system test completed:', pluginResult);
        console.log('');
        
        // Get engine status
        console.log('27. Getting engine status...');
        const status = reEngine.getStatus();
        console.log(' Engine status:', status);
        console.log('');
        
        console.log(' All Reverse Engineering Engine tests completed successfully!');
        console.log(' Test Summary:');
        console.log('   - Binary Analysis: ');
        console.log('   - Disassembly: ');
        console.log('   - String Extraction: ');
        console.log('   - Function Analysis: ');
        console.log('   - Packing Detection: ');
        console.log('   - Obfuscation Detection: ');
        console.log('   - Malware Analysis: ');
        console.log('   - Entropy Analysis: ');
        console.log('   - Section Analysis: ');
        console.log('   - Import/Export Analysis: ');
        console.log('   - Decompilation: ');
        console.log('   - Control Flow Analysis: ');
        console.log('   - Data Flow Analysis: ');
        console.log('   - Cross-Reference Analysis: ');
        console.log('   - Pattern Matching: ');
        console.log('   - YARA Rules: ');
        console.log('   - Code Similarity: ');
        console.log('   - Vulnerability Detection: ');
        console.log('   - Anti-Analysis Detection: ');
        console.log('   - Emulation: ');
        console.log('   - Dynamic Analysis: ');
        console.log('   - Report Generation: ');
        console.log('   - Batch Analysis: ');
        console.log('   - Plugin System: ');
        console.log('   - Engine Status: ');
        
    } catch (error) {
        console.error(' Reverse Engineering Engine test failed:', error.message);
        console.error('Stack trace:', error.stack);
        process.exit(1);
    }
}

// Run the test
if (require.main === module) {
    testReverseEngineering().catch(console.error);
}

module.exports = { testReverseEngineering };
