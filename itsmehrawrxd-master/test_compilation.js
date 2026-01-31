const axios = require('axios');

async function testCompilation() {
    const testCode = `def func main() -> void {
    let x: int = 42;
    ret 0;
}`;

    try {
        console.log(' Testing EON compilation...');
        console.log('Code to test:');
        console.log(testCode);
        console.log('\n---\n');

        const response = await axios.post('http://localhost:3001/api/eon/compile', {
            code: testCode,
            filename: 'test.eon'
        }, {
            timeout: 15000,
            headers: {
                'Content-Type': 'application/json'
            }
        });

        console.log(' Compilation Result:');
        console.log('Success:', response.data.success);
        console.log('Output:', response.data.output);
        
        if (response.data.error) {
            console.log('Error:', response.data.error);
        }

    } catch (error) {
        console.log(' Test failed:');
        console.log('Error:', error.message);
        
        if (error.response) {
            console.log('Response data:', error.response.data);
        }
    }
}

testCompilation();
