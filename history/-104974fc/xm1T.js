const fs = require('fs').promises;
const path = require('path');
const AVScannerEngine = require('./backend/scanner-engine');
const PEAnalyzer = require('./backend/pe-analyzer');
const SignatureEngine = require('./backend/signature-engine');

/**
 * Test Suite for Professional AV Scanner
 * Tests all detection capabilities
 */

class TestSuite {
  constructor() {
    this.scanner = new AVScannerEngine();
    this.peAnalyzer = new PEAnalyzer();
    this.signatureEngine = new SignatureEngine();
    this.testResults = [];
  }

  /**
   * Generate test file with malware signatures
   */
  async generateTestFile(name, content) {
    const testDir = path.join(__dirname, 'test-samples');
    await fs.mkdir(testDir, { recursive: true });
    const filePath = path.join(testDir, name);
    await fs.writeFile(filePath, content);
    return filePath;
  }

  /**
   * Test 1: Signature Detection - Ransomware
   */
  async testRansomwareDetection() {
    console.log('\n📋 Test 1: Ransomware Signature Detection');
    console.log('─'.repeat(60));

    // Create test file with WannaCry signatures
    const content = Buffer.concat([
      Buffer.from('MZ'), // DOS header
      Buffer.alloc(100),
      Buffer.from('tasksche.exe'),
      Buffer.alloc(50),
      Buffer.from('.WNCRYT'),
      Buffer.alloc(50),
      Buffer.from('@WanaDecryptor@'),
      Buffer.alloc(100)
    ]);

    const filePath = await this.generateTestFile('ransomware_test.exe', content);

    try {
      const matches = await this.signatureEngine.scanFile(filePath);
      const threatScore = this.signatureEngine.getThreatScore(matches);

      console.log(`✓ File created: ${path.basename(filePath)}`);
      console.log(`✓ Signatures matched: ${matches.length}`);
      matches.forEach(match => {
        console.log(`  - ${match.name} (${match.severity}) - ${match.matchCount} matches`);
      });
      console.log(`✓ Threat Score: ${threatScore}/100`);

      const passed = matches.some(m => m.name.includes('WannaCry'));
      this.testResults.push({ name: 'Ransomware Detection', passed });
      console.log(passed ? '✅ PASS' : '❌ FAIL');
    } catch (error) {
      console.error('❌ Error:', error.message);
      this.testResults.push({ name: 'Ransomware Detection', passed: false });
    }
  }

  /**
   * Test 2: Signature Detection - Info Stealer
   */
  async testStealerDetection() {
    console.log('\n📋 Test 2: Info Stealer Signature Detection');
    console.log('─'.repeat(60));

    const content = Buffer.concat([
      Buffer.from('MZ'),
      Buffer.alloc(100),
      Buffer.from('RedLine'),
      Buffer.alloc(50),
      Buffer.from('AuthToken'),
      Buffer.alloc(50),
      Buffer.from('\\Local State'),
      Buffer.alloc(50),
      Buffer.from('\\Login Data'),
      Buffer.alloc(100)
    ]);

    const filePath = await this.generateTestFile('stealer_test.exe', content);

    try {
      const matches = await this.signatureEngine.scanFile(filePath);
      const threatScore = this.signatureEngine.getThreatScore(matches);

      console.log(`✓ File created: ${path.basename(filePath)}`);
      console.log(`✓ Signatures matched: ${matches.length}`);
      matches.forEach(match => {
        console.log(`  - ${match.name} (${match.severity}) - ${match.matchCount} matches`);
      });
      console.log(`✓ Threat Score: ${threatScore}/100`);

      const passed = matches.some(m => m.name.includes('RedLine'));
      this.testResults.push({ name: 'Stealer Detection', passed });
      console.log(passed ? '✅ PASS' : '❌ FAIL');
    } catch (error) {
      console.error('❌ Error:', error.message);
      this.testResults.push({ name: 'Stealer Detection', passed: false });
    }
  }

  /**
   * Test 3: Signature Detection - Backdoor
   */
  async testBackdoorDetection() {
    console.log('\n📋 Test 3: Backdoor Signature Detection');
    console.log('─'.repeat(60));

    const content = Buffer.concat([
      Buffer.from('MZ'),
      Buffer.alloc(100),
      Buffer.from('beacon'),
      Buffer.alloc(50),
      Buffer.from('ReflectiveLoader'),
      Buffer.alloc(50),
      Buffer.from('%c%c%c%c%c%c%c%c%cMSSE'),
      Buffer.alloc(100)
    ]);

    const filePath = await this.generateTestFile('backdoor_test.dll', content);

    try {
      const matches = await this.signatureEngine.scanFile(filePath);
      const threatScore = this.signatureEngine.getThreatScore(matches);

      console.log(`✓ File created: ${path.basename(filePath)}`);
      console.log(`✓ Signatures matched: ${matches.length}`);
      matches.forEach(match => {
        console.log(`  - ${match.name} (${match.severity}) - ${match.matchCount} matches`);
      });
      console.log(`✓ Threat Score: ${threatScore}/100`);

      const passed = matches.some(m => m.name.includes('CobaltStrike'));
      this.testResults.push({ name: 'Backdoor Detection', passed });
      console.log(passed ? '✅ PASS' : '❌ FAIL');
    } catch (error) {
      console.error('❌ Error:', error.message);
      this.testResults.push({ name: 'Backdoor Detection', passed: false });
    }
  }

  /**
   * Test 4: Signature Detection - Cryptominer
   */
  async testMinerDetection() {
    console.log('\n📋 Test 4: Cryptominer Signature Detection');
    console.log('─'.repeat(60));

    const content = Buffer.concat([
      Buffer.from('xmrig'),
      Buffer.alloc(50),
      Buffer.from('donate-level'),
      Buffer.alloc(50),
      Buffer.from('randomx'),
      Buffer.alloc(50),
      Buffer.from('pool.minexmr.com'),
      Buffer.alloc(100)
    ]);

    const filePath = await this.generateTestFile('miner_test.exe', content);

    try {
      const matches = await this.signatureEngine.scanFile(filePath);
      const threatScore = this.signatureEngine.getThreatScore(matches);

      console.log(`✓ File created: ${path.basename(filePath)}`);
      console.log(`✓ Signatures matched: ${matches.length}`);
      matches.forEach(match => {
        console.log(`  - ${match.name} (${match.severity}) - ${match.matchCount} matches`);
      });
      console.log(`✓ Threat Score: ${threatScore}/100`);

      const passed = matches.some(m => m.name.includes('XMRig'));
      this.testResults.push({ name: 'Miner Detection', passed });
      console.log(passed ? '✅ PASS' : '❌ FAIL');
    } catch (error) {
      console.error('❌ Error:', error.message);
      this.testResults.push({ name: 'Miner Detection', passed: false });
    }
  }

  /**
   * Test 5: Signature Detection - Credential Dumper
   */
  async testCredentialDumperDetection() {
    console.log('\n📋 Test 5: Credential Dumper Detection');
    console.log('─'.repeat(60));

    const content = Buffer.concat([
      Buffer.from('mimikatz'),
      Buffer.alloc(50),
      Buffer.from('sekurlsa'),
      Buffer.alloc(50),
      Buffer.from('lsadump'),
      Buffer.alloc(50),
      Buffer.from('gentilkiwi'),
      Buffer.alloc(100)
    ]);

    const filePath = await this.generateTestFile('dumper_test.exe', content);

    try {
      const matches = await this.signatureEngine.scanFile(filePath);
      const threatScore = this.signatureEngine.getThreatScore(matches);

      console.log(`✓ File created: ${path.basename(filePath)}`);
      console.log(`✓ Signatures matched: ${matches.length}`);
      matches.forEach(match => {
        console.log(`  - ${match.name} (${match.severity}) - ${match.matchCount} matches`);
      });
      console.log(`✓ Threat Score: ${threatScore}/100`);

      const passed = matches.some(m => m.name.includes('Mimikatz'));
      this.testResults.push({ name: 'Credential Dumper Detection', passed });
      console.log(passed ? '✅ PASS' : '❌ FAIL');
    } catch (error) {
      console.error('❌ Error:', error.message);
      this.testResults.push({ name: 'Credential Dumper Detection', passed: false });
    }
  }

  /**
   * Test 6: Heuristic Analysis
   */
  async testHeuristicAnalysis() {
    console.log('\n📋 Test 6: Heuristic Analysis');
    console.log('─'.repeat(60));

    // Create file with suspicious API calls and high entropy
    const highEntropyData = Buffer.alloc(1024);
    for (let i = 0; i < 1024; i++) {
      highEntropyData[i] = Math.floor(Math.random() * 256);
    }

    const content = Buffer.concat([
      Buffer.from('VirtualAllocEx'),
      Buffer.alloc(50),
      Buffer.from('WriteProcessMemory'),
      Buffer.alloc(50),
      Buffer.from('CreateRemoteThread'),
      Buffer.alloc(50),
      Buffer.from('GetAsyncKeyState'),
      Buffer.alloc(50),
      highEntropyData
    ]);

    const filePath = await this.generateTestFile('heuristic_test.exe', content);

    try {
      const analysis = await this.scanner.heuristicAnalysis(filePath);

      console.log(`✓ File created: ${path.basename(filePath)}`);
      console.log(`✓ Entropy: ${analysis.entropy.toFixed(2)}`);
      console.log(`✓ Suspicious: ${analysis.suspicious}`);
      console.log(`✓ Threat Score: ${analysis.threatScore}/100`);
      console.log(`✓ Threat Indicators:`);
      Object.entries(analysis.threatIndicators).forEach(([key, value]) => {
        console.log(`  - ${key}: ${value}`);
      });
      console.log(`✓ Detected Patterns: ${analysis.detectedPatterns.length}`);
      analysis.detectedPatterns.forEach(pattern => {
        console.log(`  - ${pattern}`);
      });

      const passed = analysis.suspicious && analysis.threatScore > 30;
      this.testResults.push({ name: 'Heuristic Analysis', passed });
      console.log(passed ? '✅ PASS' : '❌ FAIL');
    } catch (error) {
      console.error('❌ Error:', error.message);
      this.testResults.push({ name: 'Heuristic Analysis', passed: false });
    }
  }

  /**
   * Test 7: Full Scanner Integration
   */
  async testFullScanner() {
    console.log('\n📋 Test 7: Full Scanner Integration');
    console.log('─'.repeat(60));

    // Create comprehensive test file
    const content = Buffer.concat([
      Buffer.from('MZ'), // DOS header
      Buffer.alloc(100),
      Buffer.from('mimikatz'),
      Buffer.alloc(50),
      Buffer.from('VirtualAllocEx'),
      Buffer.alloc(50),
      Buffer.from('WriteProcessMemory'),
      Buffer.alloc(50),
      Buffer.from('CreateRemoteThread'),
      Buffer.alloc(100),
      Buffer.from('GetAsyncKeyState'),
      Buffer.alloc(100)
    ]);

    const filePath = await this.generateTestFile('full_test.exe', content);

    try {
      console.log('⏳ Scanning file (this may take a few seconds)...\n');
      const results = await this.scanner.scanFile(filePath);

      console.log(`✓ Scan ID: ${results.scanId}`);
      console.log(`✓ File Hash: ${results.fileHash}`);
      console.log(`✓ Total Engines: ${results.totalEngines}`);
      console.log(`✓ Detections: ${results.detections}`);
      console.log(`✓ Detection Rate: ${((results.detections / results.totalEngines) * 100).toFixed(1)}%`);
      console.log(`✓ Risk Level: ${results.riskLevel.toUpperCase()}`);
      console.log(`✓ Threat Score: ${results.heuristics.threatScore}/100`);

      if (results.signatureMatches.length > 0) {
        console.log(`✓ Signature Matches: ${results.signatureMatches.length}`);
        results.signatureMatches.forEach(match => {
          console.log(`  - ${match.name} (${match.severity})`);
        });
      }

      console.log(`\n✓ Engine Results:`);
      const detectedEngines = results.engines.filter(e => e.detected);
      console.log(`  Detected by ${detectedEngines.length} engines:`);
      detectedEngines.slice(0, 10).forEach(engine => {
        console.log(`  - ${engine.engine}: ${engine.threatName}`);
      });
      if (detectedEngines.length > 10) {
        console.log(`  ... and ${detectedEngines.length - 10} more`);
      }

      const passed = results.detections > 0 && results.riskLevel !== 'clean';
      this.testResults.push({ name: 'Full Scanner Integration', passed });
      console.log(passed ? '\n✅ PASS' : '\n❌ FAIL');
    } catch (error) {
      console.error('❌ Error:', error.message);
      this.testResults.push({ name: 'Full Scanner Integration', passed: false });
    }
  }

  /**
   * Test 8: Clean File Detection
   */
  async testCleanFile() {
    console.log('\n📋 Test 8: Clean File Detection (False Positive Test)');
    console.log('─'.repeat(60));

    // Create benign file with no malicious indicators
    const content = Buffer.concat([
      Buffer.from('MZ'), // DOS header
      Buffer.alloc(100),
      Buffer.from('Hello World'),
      Buffer.alloc(100),
      Buffer.from('This is a clean file for testing'),
      Buffer.alloc(500)
    ]);

    const filePath = await this.generateTestFile('clean_test.exe', content);

    try {
      const results = await this.scanner.scanFile(filePath);

      console.log(`✓ File Hash: ${results.fileHash}`);
      console.log(`✓ Detections: ${results.detections}/${results.totalEngines}`);
      console.log(`✓ Detection Rate: ${((results.detections / results.totalEngines) * 100).toFixed(1)}%`);
      console.log(`✓ Risk Level: ${results.riskLevel.toUpperCase()}`);
      console.log(`✓ Threat Score: ${results.heuristics.threatScore}/100`);

      // Clean file should have low detection rate
      const detectionRate = results.detections / results.totalEngines;
      const passed = detectionRate < 0.2 && results.heuristics.threatScore < 30;
      this.testResults.push({ name: 'Clean File Detection', passed });
      console.log(passed ? '✅ PASS (Low false positive rate)' : '❌ FAIL (High false positive rate)');
    } catch (error) {
      console.error('❌ Error:', error.message);
      this.testResults.push({ name: 'Clean File Detection', passed: false });
    }
  }

  /**
   * Print final test summary
   */
  printSummary() {
    console.log('\n' + '═'.repeat(60));
    console.log('🏁 TEST SUMMARY');
    console.log('═'.repeat(60));

    const passed = this.testResults.filter(r => r.passed).length;
    const total = this.testResults.length;
    const percentage = ((passed / total) * 100).toFixed(1);

    this.testResults.forEach(result => {
      console.log(`${result.passed ? '✅' : '❌'} ${result.name}`);
    });

    console.log('\n' + '─'.repeat(60));
    console.log(`Results: ${passed}/${total} tests passed (${percentage}%)`);
    console.log('─'.repeat(60));

    if (passed === total) {
      console.log('\n🎉 ALL TESTS PASSED! Scanner is working correctly.');
    } else {
      console.log(`\n⚠️  ${total - passed} test(s) failed. Review results above.`);
    }
  }

  /**
   * Run all tests
   */
  async runAll() {
    console.log('═'.repeat(60));
    console.log('🧪 PROFESSIONAL AV SCANNER - TEST SUITE');
    console.log('═'.repeat(60));
    console.log('Testing all detection capabilities...\n');

    await this.testRansomwareDetection();
    await this.testStealerDetection();
    await this.testBackdoorDetection();
    await this.testMinerDetection();
    await this.testCredentialDumperDetection();
    await this.testHeuristicAnalysis();
    await this.testFullScanner();
    await this.testCleanFile();

    this.printSummary();

    // Cleanup test files
    try {
      const testDir = path.join(__dirname, 'test-samples');
      await fs.rm(testDir, { recursive: true, force: true });
      console.log('\n🧹 Test files cleaned up');
    } catch (error) {
      // Ignore cleanup errors
    }
  }
}

// Run tests if executed directly
if (require.main === module) {
  const suite = new TestSuite();
  suite.runAll().catch(console.error);
}

module.exports = TestSuite;
