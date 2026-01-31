import javax.swing.*;
import javax.swing.tree.*;
import java.awt.*;
import java.awt.event.*;
import java.io.*;
import java.nio.file.*;

public class FilePanel extends JPanel {
    private JTree fileTree;
    private DefaultTreeModel treeModel;
    private JTextArea editorArea;
    private JButton newFileButton, saveButton;

    public FilePanel(String projectRoot) {
        setLayout(new BorderLayout());

        DefaultMutableTreeNode root = new DefaultMutableTreeNode(projectRoot);
        treeModel = new DefaultTreeModel(root);
        fileTree = new JTree(treeModel);
        JScrollPane treeScroll = new JScrollPane(fileTree);
        treeScroll.setPreferredSize(new Dimension(200, 0));
        add(treeScroll, BorderLayout.WEST);

        editorArea = new JTextArea();
        JScrollPane editorScroll = new JScrollPane(editorArea);
        add(editorScroll, BorderLayout.CENTER);

        JPanel buttonPanel = new JPanel();
        newFileButton = new JButton("New File");
        saveButton = new JButton("Save");
        buttonPanel.add(newFileButton);
        buttonPanel.add(saveButton);
        add(buttonPanel, BorderLayout.SOUTH);

        newFileButton.addActionListener(e -> createNewFile(projectRoot));
        saveButton.addActionListener(e -> saveCurrentFile());

        fileTree.addTreeSelectionListener(e -> openSelectedFile(projectRoot));
        refreshFileTree(root, new File(projectRoot));
    }

    private void refreshFileTree(DefaultMutableTreeNode parent, File dir) {
        parent.removeAllChildren();
        File[] files = dir.listFiles();
        if (files != null) {
            for (File file : files) {
                DefaultMutableTreeNode node = new DefaultMutableTreeNode(file.getName());
                parent.add(node);
                if (file.isDirectory()) {
                    refreshFileTree(node, file);
                }
            }
        }
        treeModel.reload();
    }

    private void openSelectedFile(String projectRoot) {
        TreePath path = fileTree.getSelectionPath();
        if (path == null) return;
        StringBuilder sb = new StringBuilder(projectRoot);
        Object[] nodes = path.getPath();
        for (int i = 1; i < nodes.length; i++) {
            sb.append(File.separator).append(nodes[i].toString());
        }
        File file = new File(sb.toString());
        if (file.isFile()) {
            try {
                String content = new String(Files.readAllBytes(file.toPath()));
                editorArea.setText(content);
                editorArea.putClientProperty("currentFile", file);
            } catch (Exception ex) {
                JOptionPane.showMessageDialog(this, "Error opening file: " + ex.getMessage());
            }
        }
    }

    private void createNewFile(String projectRoot) {
        String fileName = JOptionPane.showInputDialog(this, "Enter new file name:");
        if (fileName != null && !fileName.trim().isEmpty()) {
            try {
                Path filePath = Paths.get(projectRoot, fileName);
                Files.write(filePath, "".getBytes());
                refreshFileTree((DefaultMutableTreeNode) treeModel.getRoot(), new File(projectRoot));
            } catch (Exception ex) {
                JOptionPane.showMessageDialog(this, "Error creating file: " + ex.getMessage());
            }
        }
    }

    private void saveCurrentFile() {
        File file = (File) editorArea.getClientProperty("currentFile");
        if (file != null) {
            try {
                Files.write(file.toPath(), editorArea.getText().getBytes());
                JOptionPane.showMessageDialog(this, "File saved: " + file.getName());
            } catch (Exception ex) {
                JOptionPane.showMessageDialog(this, "Error saving file: " + ex.getMessage());
            }
        }
    }
}
