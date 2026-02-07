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
        bottomPanel.add(statusBar);
        
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
            
            Path source = lastDropped.toPath();
            String fileName = source.getFileName().toString();
            String ext = chooseExtension();
            Path outFile = workspace.resolve("EmbeddedRun." + ext);
            
            if (fileName.endsWith(".java")) {
                compileJava(source, outFile);
            } else if (fileName.endsWith(".cpp") || fileName.endsWith(".c")) {
                compileCpp(source, outFile);
            } else if (fileName.endsWith(".asm") || fileName.endsWith(".s")) {
                compileAsm(source, outFile);
            } else if (fileName.endsWith(".cs")) {
                compileCSharp(source, outFile);
            } else {
                Files.copy(source, outFile, StandardCopyOption.REPLACE_EXISTING);
            }
            
            launchExecutable(outFile);
            
            PiBeacon.log("3,14PIZL0G1C embedded-run-launched → " + outFile);
            statusBar.setText(" Launched: " + outFile.getFileName());
            
        } catch (Exception ex) {
            JOptionPane.showMessageDialog(this, "RUN failed: " + ex.getMessage());
        }
    }
    
    private void compileJava(Path source, Path output) throws Exception {
        ProcessBuilder pb = new ProcessBuilder("javac", "-cp", ".", source.toString());
        pb.directory(workspace.toFile());
        if (pb.start().waitFor() != 0) throw new IOException("javac failed");
    }
    
    private void compileCpp(Path source, Path output) throws Exception {
        ProcessBuilder pb = new ProcessBuilder("g++", "-o", output.toString(), source.toString());
        pb.directory(workspace.toFile());
        if (pb.start().waitFor() != 0) throw new IOException("g++ failed");
    }
    
    private void compileAsm(Path source, Path output) throws Exception {
        Path bootloader = Paths.get("bootloader.bin");
        if (!Files.exists(bootloader)) {
            createBootloader(bootloader);
        }
        
        byte[] asmBytes = Files.readAllBytes(source);
        byte[] bootBytes = Files.readAllBytes(bootloader);
        
        ByteArrayOutputStream combined = new ByteArrayOutputStream();
        combined.write(bootBytes);
        combined.write(asmBytes);
        
        Files.write(output, combined.toByteArray());
    }
    
    private void compileCSharp(Path source, Path output) throws Exception {
        Path roslynBox = Paths.get("RoslynBox");
        if (Files.exists(roslynBox.resolve("RoslynBoxEngine.dll"))) {
            ProcessBuilder pb = new ProcessBuilder(
                "dotnet", "exec", roslynBox.resolve("RoslynBoxEngine.dll").toString(),
                source.toString(), output.toString()
            );
            pb.directory(workspace.toFile());
            if (pb.start().waitFor() != 0) throw new IOException("RoslynBox failed");
        }
    }
    
    private void createBootloader(Path bootloader) throws IOException {
        byte[] boot = new byte[512];
        boot[510] = (byte)0x55;
        boot[511] = (byte)0xAA;
        Files.write(bootloader, boot);
    }
    
    private void launchExecutable(Path executable) throws IOException {
        new ProcessBuilder(executable.toString())
            .directory(workspace.toFile())
            .redirectError(ProcessBuilder.Redirect.INHERIT)
            .redirectOutput(ProcessBuilder.Redirect.INHERIT)
            .start();
    }
    
    private String chooseExtension() {
        String[] exts = {"exe", "bin", "elf", "com", "dll", "so", "wasm", "class"};
        String result = (String) JOptionPane.showInputDialog(this,
            "Output format:", "RUN π",
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