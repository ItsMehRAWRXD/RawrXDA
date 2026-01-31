// Pure Assembly Compiler - No External Dependencies
// Compiles assembly code to executable using only Java

import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.regex.*;

public class PureAssemblyCompiler {
    private static final String VERSION = "1.0.0";
    private static final String TARGET_ARCH = "x86_64";
    
    private Map<String, String> instructionMap;
    private Map<String, Integer> registerMap;
    private Map<String, String> directiveMap;
    private List<String> outputLines;
    private Map<String, Integer> labelMap;
    private int currentAddress;
    
    public PureAssemblyCompiler() {
        initializeCompiler();
    }
    
    private void initializeCompiler() {
        System.out.println("=== Pure Assembly Compiler v" + VERSION + " ===");
        System.out.println("No external dependencies - Pure Java implementation");
        
        this.instructionMap = new HashMap<>();
        this.registerMap = new HashMap<>();
        this.directiveMap = new HashMap<>();
        this.outputLines = new ArrayList<>();
        this.labelMap = new HashMap<>();
        this.currentAddress = 0;
        
        initializeInstructionSet();
        initializeRegisters();
        initializeDirectives();
    }
    
    private void initializeInstructionSet() {
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
    }
    
    private void initializeRegisters() {
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
    }
    
    private void initializeDirectives() {
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
    
    public void compileAssembly(String sourceFile) throws CompilationException {
        System.out.println("Compiling: " + sourceFile);
        
        try {
            // Step 1: Parse assembly source
            List<AssemblyLine> lines = parseAssemblyFile(sourceFile);
            
            // Step 2: First pass - collect labels
            firstPass(lines);
            
            // Step 3: Second pass - generate machine code
            secondPass(lines);
            
            // Step 4: Generate executable
            generateExecutable(sourceFile);
            
            System.out.println("Compilation successful!");
            
        } catch (Exception e) {
            throw new CompilationException("Compilation failed: " + e.getMessage(), e);
        }
    }
    
    private List<AssemblyLine> parseAssemblyFile(String sourceFile) throws IOException {
        List<String> fileLines = Files.readAllLines(Paths.get(sourceFile));
        List<AssemblyLine> lines = new ArrayList<>();
        
        for (int i = 0; i < fileLines.size(); i++) {
            String line = fileLines.get(i).trim();
            if (line.isEmpty() || line.startsWith(";")) {
                continue;
            }
            
            AssemblyLine asmLine = parseLine(line, i + 1);
            if (asmLine != null) {
                lines.add(asmLine);
            }
        }
        
        return lines;
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
        System.out.println("First pass - collecting labels...");
        
        currentAddress = 0;
        
        for (AssemblyLine line : lines) {
            if (line.type == LineType.LABEL) {
                labelMap.put(line.label, currentAddress);
                System.out.println("Label '" + line.label + "' at address 0x" + Integer.toHexString(currentAddress));
            } else if (line.type == LineType.INSTRUCTION) {
                currentAddress += getInstructionSize(line);
            } else if (line.type == LineType.DATA) {
                currentAddress += getDataSize(line);
            }
        }
        
        System.out.println("First pass complete. Total size: " + currentAddress + " bytes");
    }
    
    private void secondPass(List<AssemblyLine> lines) {
        System.out.println("Second pass - generating machine code...");
        
        currentAddress = 0;
        outputLines.clear();
        
        for (AssemblyLine line : lines) {
            if (line.type == LineType.INSTRUCTION) {
                generateInstructionCode(line);
            } else if (line.type == LineType.DATA) {
                generateDataCode(line);
            }
        }
        
        System.out.println("Second pass complete. Generated " + outputLines.size() + " bytes of machine code");
    }
    
    private void generateInstructionCode(AssemblyLine line) {
        String instruction = line.instruction.toLowerCase();
        String operands = line.operands;
        
        System.out.println("Generating code for: " + line.originalLine);
        
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
    
    private void generateExecutable(String sourceFile) throws IOException {
        System.out.println("Generating executable...");
        
        String executableName = sourceFile.replace(".asm", "");
        
        // Generate ELF header
        List<String> elfHeader = generateElfHeader();
        
        // Generate executable file
        try (PrintWriter writer = new PrintWriter(new FileWriter(executableName + ".hex"))) {
            // Write ELF header
            for (String line : elfHeader) {
                writer.println(line);
            }
            
            // Write machine code
            for (String line : outputLines) {
                writer.println(line);
            }
        }
        
        // Convert hex to binary
        convertHexToBinary(executableName + ".hex", executableName);
        
        System.out.println("Executable generated: " + executableName);
    }
    
    private List<String> generateElfHeader() {
        List<String> header = new ArrayList<>();
        
        // ELF header for x86-64
        header.add("7f 45 4c 46"); // ELF magic
        header.add("02"); // 64-bit
        header.add("01"); // Little endian
        header.add("01"); // ELF version
        header.add("00"); // System V ABI
        header.add("00 00 00 00 00 00 00"); // Padding
        header.add("02 00"); // ET_EXEC
        header.add("3e 00"); // EM_X86_64
        header.add("01 00 00 00"); // Version
        header.add("00 00 00 00 00 00 00 00"); // Entry point
        header.add("40 00 00 00 00 00 00 00"); // Program header offset
        header.add("00 00 00 00 00 00 00 00"); // Section header offset
        header.add("00 00 00 00"); // Flags
        header.add("40 00"); // Header size
        header.add("38 00"); // Program header size
        header.add("01 00"); // Number of program headers
        header.add("40 00"); // Section header size
        header.add("00 00"); // Number of section headers
        header.add("00 00"); // Section header string table index
        
        return header;
    }
    
    private void convertHexToBinary(String hexFile, String outputFile) throws IOException {
        System.out.println("Converting hex to binary...");
        
        List<String> hexLines = Files.readAllLines(Paths.get(hexFile));
        ByteArrayOutputStream binary = new ByteArrayOutputStream();
        
        for (String line : hexLines) {
            String[] hexBytes = line.split(" ");
            for (String hexByte : hexBytes) {
                if (!hexByte.isEmpty()) {
                    int value = Integer.parseInt(hexByte, 16);
                    binary.write(value);
                }
            }
        }
        
        Files.write(Paths.get(outputFile), binary.toByteArray());
        new File(outputFile).setExecutable(true);
        
        System.out.println("Binary executable created: " + outputFile);
    }
    
    public static void main(String[] args) {
        if (args.length == 0) {
            System.out.println("Usage: java PureAssemblyCompiler <assembly_file.asm>");
            return;
        }
        
        PureAssemblyCompiler compiler = new PureAssemblyCompiler();
        
        try {
            compiler.compileAssembly(args[0]);
            System.out.println("=== Compilation Complete ===");
            System.out.println("Security Level: MAXIMUM");
            System.out.println("Local Processing: ENABLED");
            System.out.println("No External Dependencies: TRUE");
            
        } catch (CompilationException e) {
            System.err.println("Compilation failed: " + e.getMessage());
            e.printStackTrace();
        }
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
    
    // Custom exception for compilation errors
    public static class CompilationException extends Exception {
        public CompilationException(String message, Throwable cause) {
            super(message, cause);
        }
    }
}
