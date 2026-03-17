#include <iostream>
#include <vector>
#include <string>
#include "re_tools.h"

int main() {
    std::cout << "RawrXD PE Writer Test" << std::endl;

    // Test the PE dumper first
    std::string dumpResult = dump_pe("C:\\Windows\\System32\\notepad.exe");
    std::cout << "PE Dump test:\n" << dumpResult << std::endl;

    // Test the compiler
    std::string asmCode = R"(
.data
msg db 'Hello from MASM!', 0

.code
main proc
    sub rsp, 28h
    lea rcx, msg
    call printf
    add rsp, 28h
    xor rax, rax
    ret
main endp

end
)";

    std::string compileResult = run_compiler(asmCode.c_str());
    std::cout << "Compiler test:\n" << compileResult << std::endl;

    std::cout << "PE Writer class is available but needs more work for full executable generation." << std::endl;

    return 0;
}