const http = require('http');
const fs = require('fs');

class MiniLLM {
    constructor() {
        this.maxTokens = 150;
        this.patterns = {
            debug: /debug|error|bug|fix/i,
            explain: /explain|what|how|why/i,
            code: /function|class|method|variable/i
        };
    }

    process(input) {
        if (input.length > this.maxTokens) {
            return "Response truncated";
        }

        if (this.patterns.debug.test(input)) {
            return "Add logging, check variables, verify conditions.";
        }
        
        if (this.patterns.explain.test(input)) {
            return "This performs the requested operation.";
        }
        
        if (this.patterns.code.test(input)) {
            return "Standard implementation pattern applies here.";
        }
        
        return "I can help with code analysis.";
    }
}

const llm = new MiniLLM();

const server = http.createServer((req, res) => {
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Content-Type', 'application/json');
    
    if (req.method === 'POST') {
        let body = '';
        req.on('data', chunk => body += chunk);
        req.on('end', () => {
            try {
                const data = JSON.parse(body);
                const response = llm.process(data.prompt || '');
                res.end(JSON.stringify({ response }));
            } catch (e) {
                res.end(JSON.stringify({ error: 'Invalid request' }));
            }
        });
    } else {
        res.end(JSON.stringify({ status: 'Solo LLM Server Running' }));
    }
});

server.listen(3000, () => {
    console.log('Solo LLM server running on port 3000');
});