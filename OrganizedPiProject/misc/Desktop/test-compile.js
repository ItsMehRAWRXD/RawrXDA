const http = require('http');

const testCases = [
  {
    name: "C Hello World",
    language: "c",
    filename: "main.c",
    source: `#include <stdio.h>
int main() {
    printf("Hello, World!\\n");
    return 0;
}`
  },
  {
    name: "C++ Hello World",
    language: "cpp", 
    filename: "main.cpp",
    source: `#include <iostream>
int main() {
    std::cout << "Hello, World!" << std::endl;
    return 0;
}`
  },
  {
    name: "Java Hello World",
    language: "java",
    filename: "Main.java", 
    source: `public class Main {
    public static void main(String[] args) {
        System.out.println("Hello, World!");
    }
}`
  },
  {
    name: "JavaScript Hello World",
    language: "node",
    filename: "app.js",
    source: `console.log("Hello, World!");`
  },
  {
    name: "Python Hello World", 
    language: "python",
    filename: "main.py",
    source: `print("Hello, World!")`
  }
];

async function testCompile(testCase) {
  return new Promise((resolve, reject) => {
    const postData = JSON.stringify({
      language: testCase.language,
      filename: testCase.filename,
      source: testCase.source,
      timeout: 10
    });

    const options = {
      hostname: '127.0.0.1',
      port: 4040,
      path: '/compile',
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Content-Length': Buffer.byteLength(postData)
      }
    };

    const req = http.request(options, (res) => {
      let data = '';
      res.on('data', (chunk) => data += chunk);
      res.on('end', () => {
        try {
          const result = JSON.parse(data);
          resolve(result);
        } catch (e) {
          reject(e);
        }
      });
    });

    req.on('error', reject);
    req.write(postData);
    req.end();
  });
}

async function runTests() {
  console.log('🧪 Testing Secure Compile API...\n');
  
  for (const testCase of testCases) {
    try {
      console.log(`Testing ${testCase.name}...`);
      const result = await testCompile(testCase);
      
      if (result.ok) {
        console.log(`✅ ${testCase.name}: SUCCESS`);
        if (result.artifacts && result.artifacts.length > 0) {
          console.log(`   Artifacts: ${result.artifacts.map(a => a.name).join(', ')}`);
        }
      } else {
        console.log(`❌ ${testCase.name}: FAILED (code ${result.code})`);
        if (result.stderr) {
          console.log(`   Error: ${result.stderr.substring(0, 200)}...`);
        }
      }
    } catch (error) {
      console.log(`💥 ${testCase.name}: ERROR - ${error.message}`);
    }
    console.log('');
  }
}

runTests().catch(console.error);
