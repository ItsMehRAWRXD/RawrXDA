const stubGenerator = require('./src/engines/stub-generator.js');
const path = require('path');

async function testStubGeneration() {
    try {
        console.log('Initializing stub generator...');
        await stubGenerator.initialize({});
        
        console.log('Generating stub for calc.exe...');
        const result = await stubGenerator.generateStub('C:\\Windows\\System32\\calc.exe', {
            encryptionMethod: 'aes-256-gcm',
            stubType: 'cpp',
            includeAntiDebug: true,
            includeAntiVM: true,
            includeAntiSandbox: true
        });
        
        console.log('Stub generated successfully!');
        console.log('Output file:', result.outputPath);
        console.log('Payload size:', result.payloadSize, 'bytes');
        console.log('Encrypted size:', result.encryptedSize, 'bytes');
        console.log('Duration:', result.duration, 'ms');
        
    } catch (error) {
        console.error('Error generating stub:', error.message);
    }
}

testStubGeneration();