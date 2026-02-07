// Enhanced Assembly Syntax Highlighter
// Provides advanced syntax highlighting, error detection, and code analysis for x86-64 assembly

import java.util.*;
import java.util.regex.*;
import java.awt.*;
import java.awt.event.*;
import javax.swing.*;
import javax.swing.text.*;

public class ASMSyntaxHighlighter {
    private static final Map<String, Color> SYNTAX_COLORS = new HashMap<>();
    private static final Map<String, Pattern> SYNTAX_PATTERNS = new HashMap<>();
    private static final Set<String> INSTRUCTIONS = new HashSet<>();
    private static final Set<String> REGISTERS = new HashSet<>();
    private static final Set<String> DIRECTIVES = new HashSet<>();
    
    static {
        initializeColors();
        initializePatterns();
        initializeKeywords();
    }
    
    private static void initializeColors() {
        SYNTAX_COLORS.put("instruction", new Color(0, 100, 200));      // Blue
        SYNTAX_COLORS.put("register", new Color(200, 100, 0));        // Orange
        SYNTAX_COLORS.put("directive", new Color(150, 0, 150));        // Purple
        SYNTAX_COLORS.put("comment", new Color(0, 150, 0));           // Green
        SYNTAX_COLORS.put("string", new Color(200, 0, 0));            // Red
        SYNTAX_COLORS.put("number", new Color(0, 150, 150));         // Cyan
        SYNTAX_COLORS.put("label", new Color(150, 150, 0));           // Yellow
        SYNTAX_COLORS.put("error", new Color(255, 0, 0));            // Bright Red
        SYNTAX_COLORS.put("warning", new Color(255, 165, 0));        // Orange
    }
    
    private static void initializePatterns() {
        SYNTAX_PATTERNS.put("comment", Pattern.compile(";.*$", Pattern.MULTILINE));
        SYNTAX_PATTERNS.put("string", Pattern.compile("\"[^\"]*\"|'[^']*'"));
        SYNTAX_PATTERNS.put("number", Pattern.compile("\\b(0x[0-9a-fA-F]+|[0-9]+)\\b"));
        SYNTAX_PATTERNS.put("label", Pattern.compile("^\\s*[a-zA-Z_][a-zA-Z0-9_]*:"));
        SYNTAX_PATTERNS.put("directive", Pattern.compile("^\\s*\\.(data|text|bss|section|global|extern|equ|times|db|dw|dd|dq|resb|resw|resd|resq)"));
        SYNTAX_PATTERNS.put("instruction", Pattern.compile("\\b(mov|add|sub|mul|div|inc|dec|cmp|test|jmp|je|jne|jl|jg|call|ret|push|pop|lea|and|or|xor|not|shl|shr|rol|ror|nop|hlt|int|syscall)\\b"));
        SYNTAX_PATTERNS.put("register", Pattern.compile("\\b(rax|rbx|rcx|rdx|rsi|rdi|rbp|rsp|r8|r9|r10|r11|r12|r13|r14|r15|eax|ebx|ecx|edx|esi|edi|ebp|esp|ax|bx|cx|dx|si|di|bp|sp|al|bl|cl|dl|ah|bh|ch|dh)\\b"));
    }
    
    private static void initializeKeywords() {
        // x86-64 Instructions
        INSTRUCTIONS.addAll(Arrays.asList(
            "mov", "add", "sub", "mul", "div", "inc", "dec", "cmp", "test",
            "jmp", "je", "jne", "jl", "jg", "jle", "jge", "ja", "jb", "jae", "jbe",
            "call", "ret", "push", "pop", "lea", "and", "or", "xor", "not",
            "shl", "shr", "rol", "ror", "nop", "hlt", "int", "syscall",
            "cmov", "set", "bt", "bts", "btr", "btc", "bsf", "bsr",
            "lods", "stos", "movs", "cmps", "scas", "rep", "repe", "repne",
            "fld", "fst", "fadd", "fsub", "fmul", "fdiv", "fsqrt", "fsin", "fcos",
            "xchg", "bswap", "cpuid", "rdtsc", "rdtscp", "pause", "lfence", "mfence", "sfence"
        ));
        
        // x86-64 Registers
        REGISTERS.addAll(Arrays.asList(
            "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp",
            "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
            "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp",
            "ax", "bx", "cx", "dx", "si", "di", "bp", "sp",
            "al", "bl", "cl", "dl", "ah", "bh", "ch", "dh",
            "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
            "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15",
            "ymm0", "ymm1", "ymm2", "ymm3", "ymm4", "ymm5", "ymm6", "ymm7",
            "ymm8", "ymm9", "ymm10", "ymm11", "ymm12", "ymm13", "ymm14", "ymm15"
        ));
        
        // Assembly Directives
        DIRECTIVES.addAll(Arrays.asList(
            "section", "global", "extern", "equ", "times", "db", "dw", "dd", "dq",
            "resb", "resw", "resd", "resq", "align", "bits", "default", "cpu",
            "data", "text", "bss", "rodata", "note", "comment", "debug"
        ));
    }
    
    public static void highlightText(JTextPane textPane, String text) {
        StyledDocument doc = textPane.getStyledDocument();
        
        // Clear existing styles
        doc.setCharacterAttributes(0, doc.getLength(), textPane.getStyle("default"), true);
        
        // Apply syntax highlighting
        highlightComments(doc, text);
        highlightStrings(doc, text);
        highlightNumbers(doc, text);
        highlightLabels(doc, text);
        highlightDirectives(doc, text);
        highlightInstructions(doc, text);
        highlightRegisters(doc, text);
        
        // Validate syntax and highlight errors
        validateAndHighlightErrors(doc, text);
    }
    
    private static void highlightComments(StyledDocument doc, String text) {
        Matcher matcher = SYNTAX_PATTERNS.get("comment").matcher(text);
        while (matcher.find()) {
            Style style = doc.addStyle("comment", null);
            StyleConstants.setForeground(style, SYNTAX_COLORS.get("comment"));
            StyleConstants.setItalic(style, true);
            doc.setCharacterAttributes(matcher.start(), matcher.end() - matcher.start(), style, false);
        }
    }
    
    private static void highlightStrings(StyledDocument doc, String text) {
        Matcher matcher = SYNTAX_PATTERNS.get("string").matcher(text);
        while (matcher.find()) {
            Style style = doc.addStyle("string", null);
            StyleConstants.setForeground(style, SYNTAX_COLORS.get("string"));
            doc.setCharacterAttributes(matcher.start(), matcher.end() - matcher.start(), style, false);
        }
    }
    
    private static void highlightNumbers(StyledDocument doc, String text) {
        Matcher matcher = SYNTAX_PATTERNS.get("number").matcher(text);
        while (matcher.find()) {
            Style style = doc.addStyle("number", null);
            StyleConstants.setForeground(style, SYNTAX_COLORS.get("number"));
            StyleConstants.setBold(style, true);
            doc.setCharacterAttributes(matcher.start(), matcher.end() - matcher.start(), style, false);
        }
    }
    
    private static void highlightLabels(StyledDocument doc, String text) {
        Matcher matcher = SYNTAX_PATTERNS.get("label").matcher(text);
        while (matcher.find()) {
            Style style = doc.addStyle("label", null);
            StyleConstants.setForeground(style, SYNTAX_COLORS.get("label"));
            StyleConstants.setBold(style, true);
            doc.setCharacterAttributes(matcher.start(), matcher.end() - matcher.start(), style, false);
        }
    }
    
    private static void highlightDirectives(StyledDocument doc, String text) {
        Matcher matcher = SYNTAX_PATTERNS.get("directive").matcher(text);
        while (matcher.find()) {
            Style style = doc.addStyle("directive", null);
            StyleConstants.setForeground(style, SYNTAX_COLORS.get("directive"));
            StyleConstants.setBold(style, true);
            doc.setCharacterAttributes(matcher.start(), matcher.end() - matcher.start(), style, false);
        }
    }
    
    private static void highlightInstructions(StyledDocument doc, String text) {
        Matcher matcher = SYNTAX_PATTERNS.get("instruction").matcher(text);
        while (matcher.find()) {
            Style style = doc.addStyle("instruction", null);
            StyleConstants.setForeground(style, SYNTAX_COLORS.get("instruction"));
            StyleConstants.setBold(style, true);
            doc.setCharacterAttributes(matcher.start(), matcher.end() - matcher.start(), style, false);
        }
    }
    
    private static void highlightRegisters(StyledDocument doc, String text) {
        Matcher matcher = SYNTAX_PATTERNS.get("register").matcher(text);
        while (matcher.find()) {
            Style style = doc.addStyle("register", null);
            StyleConstants.setForeground(style, SYNTAX_COLORS.get("register"));
            StyleConstants.setBold(style, true);
            doc.setCharacterAttributes(matcher.start(), matcher.end() - matcher.start(), style, false);
        }
    }
    
    private static void validateAndHighlightErrors(StyledDocument doc, String text) {
        String[] lines = text.split("\n");
        int lineStart = 0;
        
        for (int i = 0; i < lines.length; i++) {
            String line = lines[i].trim();
            int lineEnd = lineStart + line.length();
            
            // Check for common assembly errors
            if (line.contains("mov") && !isValidMovInstruction(line)) {
                highlightError(doc, lineStart, lineEnd);
            }
            
            if (line.contains("jmp") && !isValidJumpInstruction(line)) {
                highlightError(doc, lineStart, lineEnd);
            }
            
            if (line.contains("call") && !isValidCallInstruction(line)) {
                highlightError(doc, lineStart, lineEnd);
            }
            
            // Check for undefined labels
            if (line.contains(":") && !isValidLabel(line)) {
                highlightWarning(doc, lineStart, lineEnd);
            }
            
            lineStart = lineEnd + 1; // +1 for newline
        }
    }
    
    private static void highlightError(StyledDocument doc, int start, int end) {
        Style style = doc.addStyle("error", null);
        StyleConstants.setForeground(style, SYNTAX_COLORS.get("error"));
        StyleConstants.setBold(style, true);
        StyleConstants.setUnderline(style, true);
        doc.setCharacterAttributes(start, end - start, style, false);
    }
    
    private static void highlightWarning(StyledDocument doc, int start, int end) {
        Style style = doc.addStyle("warning", null);
        StyleConstants.setForeground(style, SYNTAX_COLORS.get("warning"));
        StyleConstants.setBold(style, true);
        doc.setCharacterAttributes(start, end - start, style, false);
    }
    
    private static boolean isValidMovInstruction(String line) {
        // Basic validation for mov instruction
        return line.matches(".*mov\\s+[a-zA-Z0-9_]+\\s*,\\s*[a-zA-Z0-9_]+.*");
    }
    
    private static boolean isValidJumpInstruction(String line) {
        // Basic validation for jump instruction
        return line.matches(".*jmp\\s+[a-zA-Z0-9_]+.*");
    }
    
    private static boolean isValidCallInstruction(String line) {
        // Basic validation for call instruction
        return line.matches(".*call\\s+[a-zA-Z0-9_]+.*");
    }
    
    private static boolean isValidLabel(String line) {
        // Basic validation for label
        return line.matches("^\\s*[a-zA-Z_][a-zA-Z0-9_]*\\s*:.*");
    }
    
    public static List<String> getCompletions(String prefix) {
        List<String> completions = new ArrayList<>();
        
        // Add instruction completions
        for (String instruction : INSTRUCTIONS) {
            if (instruction.toLowerCase().startsWith(prefix.toLowerCase())) {
                completions.add(instruction);
            }
        }
        
        // Add register completions
        for (String register : REGISTERS) {
            if (register.toLowerCase().startsWith(prefix.toLowerCase())) {
                completions.add(register);
            }
        }
        
        // Add directive completions
        for (String directive : DIRECTIVES) {
            if (directive.toLowerCase().startsWith(prefix.toLowerCase())) {
                completions.add("." + directive);
            }
        }
        
        return completions;
    }
    
    public static String getInstructionHelp(String instruction) {
        Map<String, String> helpMap = new HashMap<>();
        
        helpMap.put("mov", "MOV destination, source - Move data from source to destination");
        helpMap.put("add", "ADD destination, source - Add source to destination");
        helpMap.put("sub", "SUB destination, source - Subtract source from destination");
        helpMap.put("mul", "MUL source - Multiply accumulator by source");
        helpMap.put("div", "DIV source - Divide accumulator by source");
        helpMap.put("jmp", "JMP target - Unconditional jump to target");
        helpMap.put("call", "CALL target - Call subroutine at target");
        helpMap.put("ret", "RET - Return from subroutine");
        helpMap.put("push", "PUSH source - Push source onto stack");
        helpMap.put("pop", "POP destination - Pop from stack to destination");
        
        return helpMap.getOrDefault(instruction.toLowerCase(), "No help available for: " + instruction);
    }
}
