package com.example;

import java.io.*;
import java.nio.file.*;
import java.util.*;

public class NativeCppCompiler {
    public static void main(String[] args) throws Exception {
        if (args.length < 2) {
            System.err.println("Usage: java -jar compiler.jar <cpp-file> <output-file>");
            System.exit(1);
        }

        String cppFile = args[0];
        String outputFile = args[1];

        System.out.println("π Native C++ Compilation with Roslyn");
        System.out.println("=====================================");

        // Read C++ source
        String cppSource = Files.readString(Paths.get(cppFile));
        System.out.println("✓ Read C++ source: " + cppFile);

        // Generate intermediate representation
        String intermediateCode = generateIntermediateCode(cppSource);
        System.out.println("✓ Generated intermediate code");

        // Use system compiler (gcc/g++) for native compilation
        compileNative(intermediateCode, outputFile);
        System.out.println("✓ Native compilation complete: " + outputFile);

        System.out.println("π Native compilation complete!");
    }

    private static String generateIntermediateCode(String cppSource) {
        // Convert C++ to compilable form
        StringBuilder intermediate = new StringBuilder();

        intermediate.append("#include <iostream>\n");
        intermediate.append("#include <cstdio>\n\n");

        String[] lines = cppSource.split("\n");
        for (String line : lines) {
            line = line.trim();
            if (line.startsWith("#include")) {
                intermediate.append(line).append("\n");
            } else if (line.contains("int main(")) {
                intermediate.append("int main() {\n");
            } else if (line.contains("cout <<")) {
                // Convert cout to printf
                String content = line.substring(line.indexOf("<<") + 2);
                if (content.contains("<< endl")) {
                    content = content.replace(" << endl", "\\n");
                }
                intermediate.append("    printf(\"").append(content.trim()).append("\");\n");
            } else if (!line.isEmpty() && !line.equals("}")) {
                intermediate.append("    ").append(line).append("\n");
            } else if (line.equals("}")) {
                intermediate.append("    return 0;\n");
                intermediate.append("}\n");
            }
        }

        return intermediate.toString();
    }

    private static void compileNative(String code, String outputFile) throws Exception {
        // Write intermediate code to temp file
        Path tempFile = Files.createTempFile("native_cpp", ".cpp");
        Files.writeString(tempFile, code);

        try {
            // Use system compiler
            String compiler = System.getProperty("os.name").toLowerCase().contains("win") ? "g++" : "g++";

            ProcessBuilder pb = new ProcessBuilder(
                compiler,
                tempFile.toString(),
                "-o",
                outputFile,
                "-std=c++11"
            );

            Process p = pb.start();

            // Wait for compilation
            int exitCode = p.waitFor();

            if (exitCode != 0) {
                // Read error output
                try (InputStream errorStream = p.getErrorStream()) {
                    String error = new String(errorStream.readAllBytes());
                    throw new IOException("Compilation failed: " + error);
                }
            }

            // Make executable on Unix-like systems
            if (!System.getProperty("os.name").toLowerCase().contains("win")) {
                new ProcessBuilder("chmod", "+x", outputFile).start().waitFor();
            }

        } finally {
            // Clean up temp file
            Files.deleteIfExists(tempFile);
        }
    }
}