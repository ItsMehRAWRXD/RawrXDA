#!/usr/bin/env node
'use strict';

const { exec } = require('child_process');
const fs = require('fs');
const path = require('path');

/**
 * Security audit script for RawrZ
 * Runs npm audit and provides recommendations
 */

function runAudit() {
  console.log('🔍 Running security audit...\n');
  
  exec('npm audit --json', (error, stdout, stderr) => {
    if (error) {
      console.error('❌ Audit failed:', error.message);
      return;
    }
    
    try {
      const auditResult = JSON.parse(stdout);
      const vulnerabilities = auditResult.metadata?.vulnerabilities || {};
      const total = vulnerabilities.total || 0;
      
      console.log('📊 Security Audit Results:');
      console.log('========================');
      console.log(`Total vulnerabilities: ${total}`);
      console.log(`High: ${vulnerabilities.high || 0}`);
      console.log(`Moderate: ${vulnerabilities.moderate || 0}`);
      console.log(`Low: ${vulnerabilities.low || 0}`);
      console.log(`Info: ${vulnerabilities.info || 0}\n`);
      
      if (total === 0) {
        console.log('✅ No vulnerabilities found!');
      } else {
        console.log('⚠️  Vulnerabilities detected:');
        
        if (auditResult.vulnerabilities) {
          Object.entries(auditResult.vulnerabilities).forEach(([pkg, vuln]) => {
            console.log(`\n📦 ${pkg}`);
            console.log(`   Severity: ${vuln.severity}`);
            console.log(`   Title: ${vuln.title}`);
            console.log(`   Recommendation: ${vuln.recommendation || 'Update package'}`);
          });
        }
        
        console.log('\n🔧 Recommended actions:');
        console.log('1. Run: npm audit fix');
        console.log('2. Review and update vulnerable packages');
        console.log('3. Consider using npm audit fix --force for major updates');
        console.log('4. Test thoroughly after updates\n');
      }
      
      // Save audit report
      const reportPath = path.join(__dirname, '..', 'audit-report.json');
      fs.writeFileSync(reportPath, JSON.stringify(auditResult, null, 2));
      console.log(`📄 Full report saved to: ${reportPath}`);
      
    } catch (parseError) {
      console.error('❌ Failed to parse audit results:', parseError.message);
      console.log('Raw output:', stdout);
    }
  });
}

function checkDependencies() {
  console.log('📋 Checking dependencies...\n');
  
  const packagePath = path.join(__dirname, '..', 'package.json');
  if (!fs.existsSync(packagePath)) {
    console.error('❌ package.json not found');
    return;
  }
  
  const pkg = JSON.parse(fs.readFileSync(packagePath, 'utf8'));
  const deps = { ...pkg.dependencies, ...pkg.devDependencies };
  
  console.log('📦 Installed dependencies:');
  Object.entries(deps).forEach(([name, version]) => {
    console.log(`   ${name}: ${version}`);
  });
  
  console.log(`\nTotal dependencies: ${Object.keys(deps).length}`);
}

function main() {
  console.log('🛡️  RawrZ Security Audit Tool');
  console.log('==============================\n');
  
  checkDependencies();
  console.log('');
  runAudit();
}

if (require.main === module) {
  main();
}

module.exports = { runAudit, checkDependencies };
