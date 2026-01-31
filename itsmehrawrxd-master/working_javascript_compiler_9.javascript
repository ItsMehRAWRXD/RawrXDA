
const fs = require('fs');

class SimpleCompiler {
    constructor() {
        this.tokens = [];
        this.ast = {};
    }
    
    tokenize(source) {
        return source.split(' ');
    }
    
    parse(tokens) {
        return {type: "program", tokens: tokens};
    }
    
    compile(source) {
        const tokens = this.tokenize(source);
        const ast = this.parse(tokens);
        return "compiled: " + tokens.length + " tokens";
    }
}

const compiler = new SimpleCompiler();
const result = compiler.compile("test source code");
console.log(`Result: ${result}`);
