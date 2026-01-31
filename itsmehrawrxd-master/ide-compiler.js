class IDECompiler {
    constructor() {
        this.config = require('./ide-toolchain.json');
    }
    
    compile(project, mode) {
        switch(mode) {
            case 'dotnet':
                return this.compileDotNet(project);
            case 'asm':
                return this.compileAssembly(project);
            case 'native':
                return this.compileNative(project);
        }
    }
    
    compileDotNet(project) {
        return `${this.config.toolchain.dotnet.path} build ${project}`;
    }
    
    compileAssembly(project) {
        return `${this.config.toolchain.nasm.path} -f win64 ${project}.asm -o ${project}.obj && ${this.config.toolchain.gcc.path} ${project}.obj -o ${project}.exe`;
    }
    
    compileNative(project) {
        return `${this.config.toolchain.gcc.path} -o ${project}.exe ${project}.c`;
    }
}

module.exports = IDECompiler;