import javax.swing.*;
import java.awt.*;
import java.awt.datatransfer.*;
import java.awt.dnd.*;
import java.awt.event.*;
import java.io.*;
import java.nio.file.*;
import java.util.*;

public class UniversalGrade extends JFrame implements DropTargetListener {
    private JTextArea editor;
    private JLabel statusBar;
    private Path workspace = Paths.get(".");
    private File lastDropped;
    
    public UniversalGrade() {
        setTitle("Universal Grade - π Engine");
        setDefaultCloseOperation(EXIT_ON_CLOSE);
        setSize(800, 600);
        
        editor = new JTextArea();
        editor.setFont(new Font("Consolas", Font.PLAIN, 14));
        
        JPanel bottomPanel = new JPanel(new FlowLayout());
        JButton runBtn = new JButton("RUN π");
        runBtn.addActionListener(e -> embeddedRun());
        bottomPanel.add(runBtn);
        
        statusBar = new JLabel(" Ready - Drop files here");
        statusBar.setBorder(BorderFactory.createLoweredBevelBorder());
        
        JPanel bottomPanel = new JPanel(new BorderLayout());
        bottomPanel.add(statusBar, BorderLayout.CENTER);
        
        JButton runBtn = new JButton("RUN π");
        runBtn.addActionListener(e -> embeddedRun());
        bottomPanel.add(runBtn, BorderLayout.EAST);
        
        add(new JScrollPane(editor), BorderLayout.CENTER);
        add(bottomPanel, BorderLayout.SOUTH);
        
        new DropTarget(this, this);
        setLocationRelativeTo(null);
    }
    
    private void embeddedRun() {
        if (lastDropped == null) {
            JOptionPane.showMessageDialog(this, "Nothing to run!");
            return;
        }
        try {
            PiBeacon.log("3,14PIZL0G1C embedded-run-start");
            
            Path source = decompressIfNeeded(lastDropped.toPath());
            String fileName = lastDropped.getName().toLowerCase();
            String ext = chooseExtension();
            Path outFile = workspace.resolve("EmbeddedRun." + ext);
            
            byte[] compiled = null;
            
            // Multi-language π-engine compilation
            if (fileName.endsWith(".java")) {
                compiled = compileJava(source);
            } else if (fileName.endsWith(".c") || fileName.endsWith(".cpp") || fileName.endsWith(".cc")) {
                compiled = compileCpp(source, fileName.endsWith(".cpp") || fileName.endsWith(".cc"));
            } else if (fileName.endsWith(".asm") || fileName.endsWith(".s")) {
                compiled = compileAsm(source);
            } else if (fileName.endsWith(".cs")) {
                compiled = compileCs(source);
            } else {
                // Default to C# RoslynBox for unknown extensions
                compiled = compileCs(source);
            }
            
            PiCompiler.INSTANCE.generate(Map.of(
                "source.rawrz", compiled,
                "outName", outFile.toString()
            ));
            
            new ProcessBuilder(outFile.toString())
                .directory(workspace.toFile())
                .redirectError(ProcessBuilder.Redirect.INHERIT)
                .redirectOutput(ProcessBuilder.Redirect.INHERIT)
                .start();
            
            PiBeacon.log("3,14PIZL0G1C embedded-run-launched → " + outFile);
            statusBar.setText(" Launched: " + outFile.getFileName());
            
        } catch (Exception ex) {
            JOptionPane.showMessageDialog(this, "RUN failed: " + ex.getMessage());
        }
    }
    
    private Path decompressIfNeeded(Path file) throws IOException {
        if (file.toString().endsWith(".rawrz")) {
            // Decompress RawrZ format
            return file; // Simplified - would implement actual decompression
        }
        return file;
    }
    
    private byte[] compileWithRoslynBox(String csharp, Path boxDir) throws Exception {
        ProcessBuilder pb = new ProcessBuilder(
            "dotnet", "exec", "--runtimeconfig", boxDir.resolve("RoslynBox.runtimeconfig.json").toString(),
            boxDir.resolve("RoslynBoxEngine.dll").toString(),
            csharp, "EmbeddedRun"
        );
        pb.directory(boxDir.toFile());
        Process p = pb.start();
        if (p.waitFor() != 0) throw new IOException("RoslynBox compile failed");
        try (InputStream in = p.getInputStream()) {
            return in.readAllBytes();
        }
    }
    
    private byte[] compileJava(Path source) throws Exception {
        PiBeacon.log("3,14PIZL0G1C compiling-java");
        Path tempDir = Files.createTempDirectory("pi-java");
        Path classFile = tempDir.resolve("Runner.class");
        
        ProcessBuilder pb = new ProcessBuilder("javac", source.toString(), "-d", tempDir.toString());
        Process p = pb.start();
        if (p.waitFor() != 0) throw new IOException("Java compilation failed");
        
        return Files.readAllBytes(classFile);
    }
    
    private byte[] compileCpp(Path source, boolean isCpp) throws Exception {
        PiBeacon.log("3,14PIZL0G1C compiling-cpp");
        Path exeFile = Paths.get("EmbeddedRun.exe");
        
        ProcessBuilder pb = new ProcessBuilder(
            "g++", source.toString(), "-o", exeFile.toString(), 
            isCpp ? "-lstdc++" : ""
        );
        Process p = pb.start();
        if (p.waitFor() != 0) throw new IOException("C/C++ compilation failed");
        
        return Files.readAllBytes(exeFile);
    }
    
    private byte[] compileAsm(Path source) throws Exception {
        PiBeacon.log("3,14PIZL0G1C compiling-asm");
        // Embedded bootloader - no external assembler needed
        String asm = Files.readString(source);
        byte[] bootloader = new byte[512];
        
        // Simple bootloader generation (would be more sophisticated in real implementation)
        bootloader[0] = (byte) 0xEB; // JMP instruction
        bootloader[1] = (byte) 0x3C; // Offset to boot code
        
        // Add basic boot code
        String bootCode = "\n[BITS 16]\n[ORG 0x7C00]\n\njmp start\n\nstart:\n    mov ah, 0x0E\n    mov al, 'π'\n    int 0x10\n    hlt\n\ntimes 510-($-$$) db 0\ndw 0xAA55";
        bootloader[62] = 'π'; // π character in boot sector
        
        return bootloader;
    }
    
    private byte[] compileCs(Path source) throws Exception {
        PiBeacon.log("3,14PIZL0G1C compiling-cs");
        Path roslynBox = Paths.get("RoslynBox");
        
        if (!Files.exists(roslynBox.resolve("RoslynBoxEngine.dll"))) {
            throw new FileNotFoundException("RoslynBox not found – drop the 5 MB bundle first");
        }
        
        String csharp = Files.readString(source);
        return compileWithRoslynBox(csharp, roslynBox);
    }
    
    private String chooseExtension() {
        String[] exts = {"exe", "bin", "elf", "com", "dll", "so", "wasm", "class", "jar", ""};
        String result = (String) JOptionPane.showInputDialog(this,
            "Launch extension:", "RUN π",
            JOptionPane.QUESTION_MESSAGE, null, exts, "exe");
        return result != null ? result : "exe";
    }
    
    @Override
    public void drop(DropTargetDropEvent dtde) {
        try {
            dtde.acceptDrop(DnDConstants.ACTION_COPY);
            java.util.List<File> files = (java.util.List<File>) dtde.getTransferable()
                .getTransferData(DataFlavor.javaFileListFlavor);
            
            if (!files.isEmpty()) {
                lastDropped = files.get(0);
                String content = Files.readString(lastDropped.toPath());
                editor.setText(content);
                statusBar.setText(" Dropped: " + lastDropped.getName());
            }
        } catch (Exception e) {
            statusBar.setText(" Drop failed: " + e.getMessage());
        }
    }
    
    @Override public void dragEnter(DropTargetDragEvent dtde) {}
    @Override public void dragOver(DropTargetDragEvent dtde) {}
    @Override public void dropActionChanged(DropTargetDragEvent dtde) {}
    @Override public void dragExit(DropTargetEvent dte) {}
    
    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> new UniversalGrade().setVisible(true));
    }
}

class PiBeacon {
    public static void log(String message) {
        System.out.println("π " + message);
    }
}

class PiCompiler {
    public static final PiCompiler INSTANCE = new PiCompiler();
    
    public void generate(Map<String, Object> config) throws IOException {
        String outName = (String) config.get("outName");
        byte[] source = (byte[]) config.get("source.rawrz");
        
        // Multi-format π-engine output generation
        if (outName.endsWith(".exe") || outName.endsWith(".dll") || outName.endsWith(".bin")) {
            // Direct binary output
            Files.write(Paths.get(outName), source);
        } else if (outName.endsWith(".elf")) {
            // ELF format (simplified)
            generateElf(source, outName);
        } else if (outName.endsWith(".com")) {
            // COM format (DOS executable)
            generateCom(source, outName);
        } else if (outName.endsWith(".wasm")) {
            // WebAssembly format
            generateWasm(source, outName);
        } else if (outName.endsWith(".class")) {
            // Java class file
            Files.write(Paths.get(outName), source);
        } else if (outName.endsWith(".jar")) {
            // JAR format
            generateJar(source, outName);
        } else if (outName.endsWith(".so")) {
            // Shared object (simplified)
            Files.write(Paths.get(outName), source);
        } else {
            // Default binary output
            Files.write(Paths.get(outName), source);
        }
        
        PiBeacon.log("3,14PIZL0G1C π-compiler-generated → " + outName);
    }
    
    private void generateElf(byte[] source, String outName) throws IOException {
        // Simplified ELF header generation
        byte[] elf = new byte[source.length + 64];
        // ELF magic: 0x7F 'E' 'L' 'F'
        elf[0] = (byte) 0x7F; elf[1] = 'E'; elf[2] = 'L'; elf[3] = 'F';
        // 64-bit, little-endian, executable
        elf[4] = 2; elf[5] = 1; elf[6] = 1;
        System.arraycopy(source, 0, elf, 64, source.length);
        Files.write(Paths.get(outName), elf);
    }
    
    private void generateCom(byte[] source, String outName) throws IOException {
        // DOS COM format (simplified)
        byte[] com = new byte[Math.min(source.length, 65535)]; // Max 64KB
        com[0] = (byte) 0xEB; com[1] = (byte) 0x3C; // JMP to start
        System.arraycopy(source, 0, com, 2, Math.min(source.length - 2, com.length - 2));
        Files.write(Paths.get(outName), com);
    }
    
    private void generateWasm(byte[] source, String outName) throws IOException {
        // WebAssembly magic and version
        byte[] wasm = new byte[source.length + 8];
        wasm[0] = 0x00; wasm[1] = 0x61; wasm[2] = 0x73; wasm[3] = 0x6D; // "\0asm"
        wasm[4] = 0x01; wasm[5] = 0x00; wasm[6] = 0x00; wasm[7] = 0x00; // Version 1
        System.arraycopy(source, 0, wasm, 8, source.length);
        Files.write(Paths.get(outName), wasm);
    }
    
    private void generateJar(byte[] source, String outName) throws IOException {
        // Simplified JAR (ZIP format with manifest)
        // In real implementation, would create proper JAR structure
        Files.write(Paths.get(outName), source);
    }
}