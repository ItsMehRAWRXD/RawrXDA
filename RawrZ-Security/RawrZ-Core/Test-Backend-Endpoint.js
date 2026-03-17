// Test-Backend-Endpoint.js - Add this to your RawrZ-Chat-Backend.js
// This adds a /test-models endpoint for comprehensive testing

const express = require('express');
const app = express();

// Add this endpoint to your existing backend
app.get('/test-models', async (req, res) => {
    const models = ['BigDaddyG', 'Reader', 'Coder', 'ASM-Expert', 'Java-Specialist'];
    const results = {};
    
    const testQueries = [
        { type: 'Code Generation', query: 'Write a simple C++ function to calculate factorial.' },
        { type: 'Explanation', query: 'Explain how AES-256-GCM encryption works.' },
        { type: 'Debugging', query: 'Fix this C++ code: int main() { int a = 5; cout << a; }' },
        { type: 'Security', query: 'How do I implement secure password hashing in Node.js?' },
        { type: 'ASM', query: 'Write x86-64 assembly code to add two 64-bit integers.' }
    ];
    
    for (const model of models) {
        results[model] = {
            status: 'testing',
            tests: [],
            summary: {
                successRate: 0,
                avgResponseTime: 0,
                totalTests: testQueries.length
            }
        };
        
        let successCount = 0;
        let totalTime = 0;
        
        for (const test of testQueries) {
            const startTime = Date.now();
            try {
                // Call your model (replace with actual implementation)
                const response = await callYourModel(model, test.query);
                const endTime = Date.now();
                const duration = endTime - startTime;
                
                results[model].tests.push({
                    type: test.type,
                    query: test.query,
                    response: response.substring(0, 200) + '...', // Truncate for display
                    success: true,
                    duration: duration,
                    error: null
                });
                
                successCount++;
                totalTime += duration;
                
            } catch (error) {
                const endTime = Date.now();
                const duration = endTime - startTime;
                
                results[model].tests.push({
                    type: test.type,
                    query: test.query,
                    response: null,
                    success: false,
                    duration: duration,
                    error: error.message
                });
                
                totalTime += duration;
            }
        }
        
        results[model].summary.successRate = (successCount / testQueries.length) * 100;
        results[model].summary.avgResponseTime = totalTime / testQueries.length;
        results[model].status = 'completed';
    }
    
    res.json({
        timestamp: new Date().toISOString(),
        totalModels: models.length,
        results: results
    });
});

// Health check endpoint
app.get('/health', (req, res) => {
    res.json({
        status: 'healthy',
        timestamp: new Date().toISOString(),
        uptime: process.uptime()
    });
});

// Function to call your models (implement based on your setup)
async function callYourModel(model, query) {
    // Replace this with your actual model calling logic
    switch (model) {
        case 'BigDaddyG':
            // Call BigDaddyG executable or API
            return `BigDaddyG response to: ${query}`;
        case 'Reader':
            return `Reader model response to: ${query}`;
        case 'Coder':
            return `Coder model response to: ${query}`;
        case 'ASM-Expert':
            return `ASM Expert response to: ${query}`;
        case 'Java-Specialist':
            return `Java Specialist response to: ${query}`;
        default:
            throw new Error(`Unknown model: ${model}`);
    }
}

// If running this as a standalone test server
if (require.main === module) {
    const PORT = 11440;
    app.listen(PORT, () => {
        console.log(`Test server running on http://localhost:${PORT}`);
        console.log(`Visit http://localhost:${PORT}/test-models to run tests`);
        console.log(`Visit http://localhost:${PORT}/health for health check`);
    });
}

module.exports = { app, callYourModel };
