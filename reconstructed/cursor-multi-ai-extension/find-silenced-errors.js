const fs = require('fs');
const path = require('path');

// Patterns to detect silenced errors
const patterns = [
  { name: 'Empty Catch Block', regex: /catch\s*\(.*\)\s*{\s*}/g },
  { name: 'Console Override', regex: /console\.(error|warn)\s*=\s*function/g },
  { name: 'Ignored Promise', regex: /Promise\.(resolve|reject)\(.*\)\s*;/g },
  { name: 'Empty Error Handler', regex: /\(error\)\s*=>\s*{\s*}/g },
];

// Function to scan files recursively
function scanDirectory(dir) {
  const results = [];
  const files = fs.readdirSync(dir);

  for (const file of files) {
    const filePath = path.join(dir, file);
    const stat = fs.statSync(filePath);

    if (stat.isDirectory()) {
      results.push(...scanDirectory(filePath));
    } else if (file.endsWith('.js') || file.endsWith('.ts')) {
      results.push(filePath);
    }
  }

  return results;
}

// Function to analyze a file for silenced errors
function analyzeFile(filePath) {
  const content = fs.readFileSync(filePath, 'utf-8');
  const issues = [];

  for (const pattern of patterns) {
    const matches = content.match(pattern.regex);
    if (matches) {
      issues.push({ pattern: pattern.name, count: matches.length });
    }
  }

  return issues.length > 0 ? { filePath, issues } : null;
}

// Main function
function main() {
  const rootDir = path.resolve(__dirname, '.');
  const files = scanDirectory(rootDir);
  const results = [];

  for (const file of files) {
    const analysis = analyzeFile(file);
    if (analysis) {
      results.push(analysis);
    }
  }

  if (results.length > 0) {
    console.log('Silenced Errors Found:');
    for (const result of results) {
      console.log(`\nFile: ${result.filePath}`);
      for (const issue of result.issues) {
        console.log(`  - ${issue.pattern}: ${issue.count} occurrence(s)`);
      }
    }
  } else {
    console.log('No silenced errors found.');
  }
}

main();