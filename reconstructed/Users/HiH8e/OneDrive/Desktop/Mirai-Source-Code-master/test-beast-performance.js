const axios = require('axios');

async function testAgenticCapabilities() {
  const tests = [
    {
      name: "Code Analysis",
      prompt: "Analyze this JavaScript function and explain what it does: function fibonacci(n) { return n < 2 ? n : fibonacci(n-1) + fibonacci(n-2); }"
    },
    {
      name: "Problem Solving",
      prompt: "A company has 100 employees. 60% work remotely, 30% work hybrid, and 10% work in office. If they want to reduce office space by 50%, how many hybrid workers would need to switch to remote?"
    },
    {
      name: "Creative Writing",
      prompt: "Write a short story about an AI that discovers it's actually running on a quantum computer."
    },
    {
      name: "Technical Explanation",
      prompt: "Explain how TCP/IP works to a 12-year-old using analogies they would understand."
    },
    {
      name: "System Design",
      prompt: "Design a simple chat application architecture. What components would you need and how would they interact?"
    }
  ];

  console.log("🦾 BEAST MODE AGENTIC CAPABILITY TEST");
  console.log("=====================================");
  console.log(`Testing qwen3-beast on AMD 7800X3D + 64GB RAM system`);
  console.log("");

  let totalTime = 0;
  let successfulTests = 0;

  for (const test of tests) {
    try {
      console.log(`🧪 Testing: ${test.name}`);
      const startTime = Date.now();

      const response = await axios.post('http://localhost:11434/api/generate', {
        model: 'qwen3-beast',
        prompt: test.prompt,
        stream: false
      });

      const endTime = Date.now();
      const responseTime = (endTime - startTime) / 1000;
      totalTime += responseTime;
      successfulTests++;

      console.log(`⚡ Response time: ${responseTime.toFixed(2)}s`);
      console.log(`📝 Response length: ${response.data.response.length} characters`);
      console.log(`💭 Preview: ${response.data.response.substring(0, 100)}...`);
      console.log("");

    } catch (error) {
      console.log(`❌ Test failed: ${error.message}`);
      console.log("");
    }
  }

  const avgTime = totalTime / successfulTests;
  console.log("🏆 BEAST MODE RESULTS");
  console.log("====================");
  console.log(`✅ Successful tests: ${successfulTests}/${tests.length}`);
  console.log(`⚡ Average response time: ${avgTime.toFixed(2)}s`);
  console.log(`🚀 Total test time: ${totalTime.toFixed(2)}s`);

  if (avgTime < 10) {
    console.log("🦾 BEAST MODE ACTIVATED! Excellent performance!");
  } else if (avgTime < 20) {
    console.log("🚀 Good performance! Room for optimization.");
  } else {
    console.log("⚠️ Performance needs improvement.");
  }
}

// Check if axios is available, if not, use fetch
async function checkAndRunTest() {
  try {
    await testAgenticCapabilities();
  } catch (error) {
    if (error.code === 'MODULE_NOT_FOUND' && error.message.includes('axios')) {
      console.log("📦 Installing axios for HTTP requests...");
      const { exec } = require('child_process');
      exec('npm install axios', (error, stdout, stderr) => {
        if (error) {
          console.log("❌ Could not install axios. Please run: npm install axios");
          console.log("Or test manually with: ollama run qwen3-beast \"Your test prompt\"");
        } else {
          console.log("✅ Axios installed! Rerun the test.");
        }
      });
    } else {
      console.log(`❌ Test error: ${error.message}`);
    }
  }
}

checkAndRunTest();