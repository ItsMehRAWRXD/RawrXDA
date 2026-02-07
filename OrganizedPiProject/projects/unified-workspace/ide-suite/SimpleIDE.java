import javax.swing.*;
import java.awt.*;
import java.awt.event.*;
import java.io.*;
import java.nio.file.*;

public class SimpleIDE extends JFrame {
    private JTextArea editor;
    private File currentFile;
    
    public SimpleIDE() {
        setTitle("Advanced Agentic IDE");
        setDefaultCloseOperation(EXIT_ON_CLOSE);
        setSize(800, 600);
        
        editor = new JTextArea();
        editor.setFont(new Font("Consolas", Font.PLAIN, 14));
        
        JMenuBar menuBar = new JMenuBar();
        JMenu fileMenu = new JMenu("File");
        
        JMenuItem newItem = new JMenuItem("New");
        newItem.addActionListener(e -> newFile());
        
        JMenuItem openItem = new JMenuItem("Open");
        openItem.addActionListener(e -> openFile());
        
        JMenuItem saveItem = new JMenuItem("Save");
        saveItem.addActionListener(e -> saveFile());
        
        fileMenu.add(newItem);
        fileMenu.add(openItem);
        fileMenu.add(saveItem);
        menuBar.add(fileMenu);
        
        setJMenuBar(menuBar);
        add(new JScrollPane(editor));
        setLocationRelativeTo(null);
    }
    
    private void newFile() {
        editor.setText("");
        currentFile = null;
        setTitle("Advanced Agentic IDE - New File");
    }
    
    private void openFile() {
        JFileChooser chooser = new JFileChooser();
        if (chooser.showOpenDialog(this) == JFileChooser.APPROVE_OPTION) {
            currentFile = chooser.getSelectedFile();
            try {
                editor.setText(Files.readString(currentFile.toPath()));
                setTitle("Advanced Agentic IDE - " + currentFile.getName());
            } catch (Exception e) {
                JOptionPane.showMessageDialog(this, "Error opening file: " + e.getMessage());
            }
        }
    }
    
    private void saveFile() {
        if (currentFile == null) {
            JFileChooser chooser = new JFileChooser();
            if (chooser.showSaveDialog(this) == JFileChooser.APPROVE_OPTION) {
                currentFile = chooser.getSelectedFile();
            } else return;
        }
        
        try {
            Files.writeString(currentFile.toPath(), editor.getText());
            setTitle("Advanced Agentic IDE - " + currentFile.getName());
            JOptionPane.showMessageDialog(this, "File saved successfully!");
        } catch (Exception e) {
            JOptionPane.showMessageDialog(this, "Error saving file: " + e.getMessage());
        }
    }
    
    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> new SimpleIDE().setVisible(true));
    }
}