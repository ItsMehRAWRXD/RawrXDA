// Integrated Assembly Compiler - Built into Secure IDE
// No external dependencies - Pure Java implementation

import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.regex.*;

public class IntegratedASMCompiler {
    private static final String VERSION = "1.0.0";
    private static final String TARGET_ARCH = "x86_64";
    
    private Map<String, String> instructionMap;
    private Map<String, Integer> registerMap;
    private Map<String, String> directiveMap;
    private List<String> outputLines;
    private Map<String, Integer> labelMap;
    private int currentAddress;
    private boolean debugMode;
    
    public IntegratedASMCompiler() {
        System.out.println("=== Integrated Assembly Compiler v" + VERSION + " ===");
        System.out.println("Built into Secure IDE - No external dependencies");
        
        this.instructionMap = new HashMap<>();
        this.registerMap = new HashMap<>();
        this.directiveMap = new HashMap<>();
        this.outputLines = new ArrayList<>();
        this.labelMap = new HashMap<>();
        this.currentAddress = 0;
        this.debugMode = false;
        
        initializeCompiler();
    }
    
    private void initializeCompiler() {
        // x86-64 instruction set mapping
        instructionMap.put("mov", "48 89");      // mov r64, r/m64
        instructionMap.put("add", "48 01");      // add r64, r/m64
        instructionMap.put("sub", "48 29");      // sub r64, r/m64
        instructionMap.put("cmp", "48 39");      // cmp r64, r/m64
        instructionMap.put("jmp", "e9");         // jmp rel32
        instructionMap.put("je", "0f 84");       // je rel32
        instructionMap.put("jne", "0f 85");     // jne rel32
        instructionMap.put("call", "e8");        // call rel32
        instructionMap.put("ret", "c3");         // ret
        instructionMap.put("push", "50");        // push r64
        instructionMap.put("pop", "58");        // pop r64
        instructionMap.put("syscall", "0f 05"); // syscall
        instructionMap.put("nop", "90");        // nop
        instructionMap.put("int", "cd");        // int imm8
        instructionMap.put("hlt", "f4");        // hlt
        
        // x86-64 register mapping
        registerMap.put("rax", 0);
        registerMap.put("rbx", 3);
        registerMap.put("rcx", 1);
        registerMap.put("rdx", 2);
        registerMap.put("rsi", 6);
        registerMap.put("rdi", 7);
        registerMap.put("rbp", 5);
        registerMap.put("rsp", 4);
        registerMap.put("r8", 8);
        registerMap.put("r9", 9);
        registerMap.put("r10", 10);
        registerMap.put("r11", 11);
        registerMap.put("r12", 12);
        registerMap.put("r13", 13);
        registerMap.put("r14", 14);
        registerMap.put("r15", 15);
        
        // 32-bit registers
        registerMap.put("eax", 0);
        registerMap.put("ebx", 3);
        registerMap.put("ecx", 1);
        registerMap.put("edx", 2);
        registerMap.put("esi", 6);
        registerMap.put("edi", 7);
        registerMap.put("ebp", 5);
        registerMap.put("esp", 4);
        
        // Directives
        directiveMap.put("BITS", "bits");
        directiveMap.put("section", "section");
        directiveMap.put("global", "global");
        directiveMap.put("extern", "extern");
        directiveMap.put("db", "db");
        directiveMap.put("dw", "dw");
        directiveMap.put("dd", "dd");
        directiveMap.put("dq", "dq");
        directiveMap.put("equ", "equ");
        directiveMap.put("times", "times");
    }
    
    public CompilationResult compileAssembly(String sourceCode) {
        System.out.println("=== Compiling Assembly Code ===");
        System.out.println("Source code length: " + sourceCode.length() + " characters");
        
        try {
            // Step 1: Parse assembly source
            List<AssemblyLine> lines = parseAssemblySource(sourceCode);
            
            // Step 2: First pass - collect labels
            firstPass(lines);
            
            // Step 3: Second pass - generate machine code
            secondPass(lines);
            
            // Step 4: Generate executable
            byte[] executable = generateExecutable();
            
            System.out.println("Compilation successful!");
            System.out.println("Generated " + executable.length + " bytes of machine code");
            
            return new CompilationResult(true, executable, outputLines, labelMap);
            
        } catch (Exception e) {
            System.err.println("Compilation failed: " + e.getMessage());
            return new CompilationResult(false, null, null, null);
        }
    }
    
    private List<AssemblyLine> parseAssemblySource(String sourceCode) {
        String[] lines = sourceCode.split("\n");
        List<AssemblyLine> assemblyLines = new ArrayList<>();
        
        for (int i = 0; i < lines.length; i++) {
            String line = lines[i].trim();
            if (line.isEmpty() || line.startsWith(";")) {
                continue;
            }
            
            AssemblyLine asmLine = parseLine(line, i + 1);
            if (asmLine != null) {
                assemblyLines.add(asmLine);
            }
        }
        
        return assemblyLines;
    }
    
    private AssemblyLine parseLine(String line, int lineNumber) {
        // Remove comments
        if (line.contains(";")) {
            line = line.substring(0, line.indexOf(";")).trim();
        }
        
        if (line.isEmpty()) {
            return null;
        }
        
        AssemblyLine asmLine = new AssemblyLine();
        asmLine.lineNumber = lineNumber;
        asmLine.originalLine = line;
        
        // Parse different types of lines
        if (line.startsWith("BITS")) {
            asmLine.type = LineType.DIRECTIVE;
            asmLine.directive = "BITS";
            asmLine.operands = line.substring(4).trim();
        } else if (line.startsWith("section")) {
            asmLine.type = LineType.DIRECTIVE;
            asmLine.directive = "section";
            asmLine.operands = line.substring(7).trim();
        } else if (line.startsWith("global")) {
            asmLine.type = LineType.DIRECTIVE;
            asmLine.directive = "global";
            asmLine.operands = line.substring(6).trim();
        } else if (line.contains(":")) {
            // Label definition
            asmLine.type = LineType.LABEL;
            asmLine.label = line.substring(0, line.indexOf(":")).trim();
        } else if (line.startsWith("db") || line.startsWith("dw") || line.startsWith("dd") || line.startsWith("dq")) {
            asmLine.type = LineType.DATA;
            asmLine.directive = line.split("\\s+")[0];
            asmLine.operands = line.substring(asmLine.directive.length()).trim();
        } else {
            // Instruction
            asmLine.type = LineType.INSTRUCTION;
            String[] parts = line.split("\\s+", 2);
            asmLine.instruction = parts[0];
            if (parts.length > 1) {
                asmLine.operands = parts[1];
            }
        }
        
        return asmLine;
    }
    
    private void firstPass(List<AssemblyLine> lines) {
        if (debugMode) System.out.println("First pass - collecting labels...");
        
        currentAddress = 0;
        
        for (AssemblyLine line : lines) {
            if (line.type == LineType.LABEL) {
                labelMap.put(line.label, currentAddress);
                if (debugMode) System.out.println("Label '" + line.label + "' at address 0x" + Integer.toHexString(currentAddress));
            } else if (line.type == LineType.INSTRUCTION) {
                currentAddress += getInstructionSize(line);
            } else if (line.type == LineType.DATA) {
                currentAddress += getDataSize(line);
            }
        }
        
        if (debugMode) System.out.println("First pass complete. Total size: " + currentAddress + " bytes");
    }
    
    private void secondPass(List<AssemblyLine> lines) {
        if (debugMode) System.out.println("Second pass - generating machine code...");
        
        currentAddress = 0;
        outputLines.clear();
        
        for (AssemblyLine line : lines) {
            if (line.type == LineType.INSTRUCTION) {
                generateInstructionCode(line);
            } else if (line.type == LineType.DATA) {
                generateDataCode(line);
            }
        }
        
        if (debugMode) System.out.println("Second pass complete. Generated " + outputLines.size() + " bytes of machine code");
    }
    
    private void generateInstructionCode(AssemblyLine line) {
        String instruction = line.instruction.toLowerCase();
        String operands = line.operands;
        
        if (debugMode) System.out.println("Generating code for: " + line.originalLine);
        
        switch (instruction) {
            case "mov":
                generateMovCode(operands);
                break;
            case "add":
                generateAddCode(operands);
                break;
            case "sub":
                generateSubCode(operands);
                break;
            case "cmp":
                generateCmpCode(operands);
                break;
            case "jmp":
                generateJmpCode(operands);
                break;
            case "je":
                generateJeCode(operands);
                break;
            case "jne":
                generateJneCode(operands);
                break;
            case "call":
                generateCallCode(operands);
                break;
            case "ret":
                generateRetCode();
                break;
            case "push":
                generatePushCode(operands);
                break;
            case "pop":
                generatePopCode(operands);
                break;
            case "syscall":
                generateSyscallCode();
                break;
            case "nop":
                generateNopCode();
                break;
            case "int":
                generateIntCode(operands);
                break;
            case "hlt":
                generateHltCode();
                break;
            default:
                System.out.println("Warning: Unknown instruction: " + instruction);
        }
    }
    
    private void generateMovCode(String operands) {
        // mov r64, r/m64
        String[] parts = operands.split(",");
        if (parts.length == 2) {
            String dest = parts[0].trim();
            String src = parts[1].trim();
            
            // Simple mov implementation
            outputLines.add("48 89"); // mov r64, r/m64
            currentAddress += 2;
            
            // Add register encoding
            if (registerMap.containsKey(dest)) {
                int reg = registerMap.get(dest);
                outputLines.add(String.format("%02x", reg));
                currentAddress += 1;
            }
        }
    }
    
    private void generateAddCode(String operands) {
        // add r64, r/m64
        outputLines.add("48 01"); // add r64, r/m64
        currentAddress += 2;
    }
    
    private void generateSubCode(String operands) {
        // sub r64, r/m64
        outputLines.add("48 29"); // sub r64, r/m64
        currentAddress += 2;
    }
    
    private void generateCmpCode(String operands) {
        // cmp r64, r/m64
        outputLines.add("48 39"); // cmp r64, r/m64
        currentAddress += 2;
    }
    
    private void generateJmpCode(String operands) {
        // jmp rel32
        outputLines.add("e9"); // jmp rel32
        currentAddress += 1;
        
        // Calculate relative address
        if (labelMap.containsKey(operands)) {
            int targetAddress = labelMap.get(operands);
            int relativeAddress = targetAddress - (currentAddress + 4);
            outputLines.add(String.format("%08x", relativeAddress));
            currentAddress += 4;
        }
    }
    
    private void generateJeCode(String operands) {
        // je rel32
        outputLines.add("0f 84"); // je rel32
        currentAddress += 2;
        
        if (labelMap.containsKey(operands)) {
            int targetAddress = labelMap.get(operands);
            int relativeAddress = targetAddress - (currentAddress + 4);
            outputLines.add(String.format("%08x", relativeAddress));
            currentAddress += 4;
        }
    }
    
    private void generateJneCode(String operands) {
        // jne rel32
        outputLines.add("0f 85"); // jne rel32
        currentAddress += 2;
        
        if (labelMap.containsKey(operands)) {
            int targetAddress = labelMap.get(operands);
            int relativeAddress = targetAddress - (currentAddress + 4);
            outputLines.add(String.format("%08x", relativeAddress));
            currentAddress += 4;
        }
    }
    
    private void generateCallCode(String operands) {
        // call rel32
        outputLines.add("e8"); // call rel32
        currentAddress += 1;
        
        if (labelMap.containsKey(operands)) {
            int targetAddress = labelMap.get(operands);
            int relativeAddress = targetAddress - (currentAddress + 4);
            outputLines.add(String.format("%08x", relativeAddress));
            currentAddress += 4;
        }
    }
    
    private void generateRetCode() {
        // ret
        outputLines.add("c3"); // ret
        currentAddress += 1;
    }
    
    private void generatePushCode(String operands) {
        // push r64
        if (registerMap.containsKey(operands)) {
            int reg = registerMap.get(operands);
            outputLines.add(String.format("50%02x", reg)); // push r64
            currentAddress += 2;
        }
    }
    
    private void generatePopCode(String operands) {
        // pop r64
        if (registerMap.containsKey(operands)) {
            int reg = registerMap.get(operands);
            outputLines.add(String.format("58%02x", reg)); // pop r64
            currentAddress += 2;
        }
    }
    
    private void generateSyscallCode() {
        // syscall
        outputLines.add("0f 05"); // syscall
        currentAddress += 2;
    }
    
    private void generateNopCode() {
        // nop
        outputLines.add("90"); // nop
        currentAddress += 1;
    }
    
    private void generateIntCode(String operands) {
        // int imm8
        outputLines.add("cd"); // int
        currentAddress += 1;
        
        try {
            int imm8 = Integer.parseInt(operands);
            outputLines.add(String.format("%02x", imm8));
            currentAddress += 1;
        } catch (NumberFormatException e) {
            System.out.println("Warning: Invalid interrupt number: " + operands);
        }
    }
    
    private void generateHltCode() {
        // hlt
        outputLines.add("f4"); // hlt
        currentAddress += 1;
    }
    
    private void generateDataCode(AssemblyLine line) {
        String directive = line.directive;
        String operands = line.operands;
        
        switch (directive) {
            case "db":
                generateDbData(operands);
                break;
            case "dw":
                generateDwData(operands);
                break;
            case "dd":
                generateDdData(operands);
                break;
            case "dq":
                generateDqData(operands);
                break;
        }
    }
    
    private void generateDbData(String operands) {
        // db - define byte
        String[] values = operands.split(",");
        for (String value : values) {
            value = value.trim();
            if (value.startsWith("'") && value.endsWith("'")) {
                // Character literal
                char c = value.charAt(1);
                outputLines.add(String.format("%02x", (int) c));
                currentAddress += 1;
            } else {
                // Numeric literal
                try {
                    int num = Integer.parseInt(value);
                    outputLines.add(String.format("%02x", num));
                    currentAddress += 1;
                } catch (NumberFormatException e) {
                    System.out.println("Warning: Invalid byte value: " + value);
                }
            }
        }
    }
    
    private void generateDwData(String operands) {
        // dw - define word
        String[] values = operands.split(",");
        for (String value : values) {
            try {
                int num = Integer.parseInt(value.trim());
                outputLines.add(String.format("%04x", num));
                currentAddress += 2;
            } catch (NumberFormatException e) {
                System.out.println("Warning: Invalid word value: " + value);
            }
        }
    }
    
    private void generateDdData(String operands) {
        // dd - define double word
        String[] values = operands.split(",");
        for (String value : values) {
            try {
                int num = Integer.parseInt(value.trim());
                outputLines.add(String.format("%08x", num));
                currentAddress += 4;
            } catch (NumberFormatException e) {
                System.out.println("Warning: Invalid double word value: " + value);
            }
        }
    }
    
    private void generateDqData(String operands) {
        // dq - define quad word
        String[] values = operands.split(",");
        for (String value : values) {
            try {
                long num = Long.parseLong(value.trim());
                outputLines.add(String.format("%016x", num));
                currentAddress += 8;
            } catch (NumberFormatException e) {
                System.out.println("Warning: Invalid quad word value: " + value);
            }
        }
    }
    
    private int getInstructionSize(AssemblyLine line) {
        // Estimate instruction size
        switch (line.instruction.toLowerCase()) {
            case "mov":
            case "add":
            case "sub":
            case "cmp":
                return 3;
            case "jmp":
            case "call":
                return 5;
            case "je":
            case "jne":
                return 6;
            case "ret":
                return 1;
            case "push":
            case "pop":
                return 2;
            case "syscall":
                return 2;
            case "nop":
                return 1;
            case "int":
                return 2;
            case "hlt":
                return 1;
            default:
                return 1;
        }
    }
    
    private int getDataSize(AssemblyLine line) {
        switch (line.directive) {
            case "db":
                return line.operands.split(",").length;
            case "dw":
                return line.operands.split(",").length * 2;
            case "dd":
                return line.operands.split(",").length * 4;
            case "dq":
                return line.operands.split(",").length * 8;
            default:
                return 0;
        }
    }
    
    private byte[] generateExecutable() {
        System.out.println("Generating executable...");
        
        // Convert hex strings to bytes
        List<Byte> bytes = new ArrayList<>();
        
        for (String line : outputLines) {
            String[] hexBytes = line.split(" ");
            for (String hexByte : hexBytes) {
                if (!hexByte.isEmpty()) {
                    int value = Integer.parseInt(hexByte, 16);
                    bytes.add((byte) value);
                }
            }
        }
        
        // Convert to byte array
        byte[] executable = new byte[bytes.size()];
        for (int i = 0; i < bytes.size(); i++) {
            executable[i] = bytes.get(i);
        }
        
        return executable;
    }
    
    public void setDebugMode(boolean debug) {
        this.debugMode = debug;
    }
    
    // Assembly line representation
    private static class AssemblyLine {
        int lineNumber;
        String originalLine;
        LineType type;
        String instruction;
        String operands;
        String directive;
        String label;
    }
    
    private enum LineType {
        INSTRUCTION, DIRECTIVE, DATA, LABEL
    }
    
    // Compilation result
    public static class CompilationResult {
        public final boolean success;
        public final byte[] executable;
        public final List<String> machineCode;
        public final Map<String, Integer> labels;
        
        public CompilationResult(boolean success, byte[] executable, List<String> machineCode, Map<String, Integer> labels) {
            this.success = success;
            this.executable = executable;
            this.machineCode = machineCode;
            this.labels = labels;
        }
    }
}
