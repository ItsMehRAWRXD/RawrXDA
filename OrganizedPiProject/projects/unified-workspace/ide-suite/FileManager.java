    // Advanced file operations and context menu
    private JPopupMenu fileMenu;
    private File selectedFile;
    private List<File> favoriteFiles = new ArrayList<>();
    private List<File> openFiles = new ArrayList<>();
    private Map<File, Long> fileHistory = new HashMap<>();
    private List<File> workspaceRoots = new ArrayList<>();

    private void setupContextMenu() {
        fileMenu = new JPopupMenu();
        JMenuItem openItem = new JMenuItem("Open");
        openItem.addActionListener(e -> openFile(selectedFile));
        JMenuItem renameItem = new JMenuItem("Rename");
        renameItem.addActionListener(e -> renameFile(selectedFile));
        JMenuItem deleteItem = new JMenuItem("Delete");
        deleteItem.addActionListener(e -> deleteFile(selectedFile));
        JMenuItem newFileItem = new JMenuItem("New File");
        newFileItem.addActionListener(e -> createNewFile(selectedFile));
        JMenuItem newFolderItem = new JMenuItem("New Folder");
        newFolderItem.addActionListener(e -> createNewFolder(selectedFile));
        JMenuItem favoriteItem = new JMenuItem("Add to Favorites");
        favoriteItem.addActionListener(e -> addFavorite(selectedFile));
        JMenuItem historyItem = new JMenuItem("Show History");
        historyItem.addActionListener(e -> showFileHistory(selectedFile));
        fileMenu.add(openItem);
        fileMenu.add(renameItem);
        fileMenu.add(deleteItem);
        fileMenu.add(newFileItem);
        fileMenu.add(newFolderItem);
        fileMenu.add(favoriteItem);
        fileMenu.add(historyItem);
    }

    private void addTreeMouseListener() {
        fileTree.addMouseListener(new MouseAdapter() {
            public void mousePressed(MouseEvent e) {
                int selRow = fileTree.getRowForLocation(e.getX(), e.getY());
                TreePath selPath = fileTree.getPathForLocation(e.getX(), e.getY());
                if (selRow != -1 && SwingUtilities.isRightMouseButton(e)) {
                    selectedFile = getFileFromPath(selPath);
                    fileMenu.show(fileTree, e.getX(), e.getY());
                }
            }
        });
    }

    private void renameFile(File file) {
        String newName = JOptionPane.showInputDialog(this, "Rename file:", file.getName());
        if (newName != null && !newName.trim().isEmpty()) {
            File newFile = new File(file.getParent(), newName);
            if (file.renameTo(newFile)) {
                refreshTree();
            } else {
                JOptionPane.showMessageDialog(this, "Rename failed.");
            }
        }
    }

    private void deleteFile(File file) {
        int confirm = JOptionPane.showConfirmDialog(this, "Delete file?", "Confirm", JOptionPane.YES_NO_OPTION);
        if (confirm == JOptionPane.YES_OPTION) {
            if (file.delete()) {
                refreshTree();
            } else {
                JOptionPane.showMessageDialog(this, "Delete failed.");
            }
        }
    }

    private void createNewFile(File dir) {
        String name = JOptionPane.showInputDialog(this, "New file name:");
        if (name != null && !name.trim().isEmpty()) {
            File newFile = new File(dir, name);
            try {
                if (newFile.createNewFile()) {
                    refreshTree();
                } else {
                    JOptionPane.showMessageDialog(this, "File creation failed.");
                }
            } catch (IOException e) {
                JOptionPane.showMessageDialog(this, "Error: " + e.getMessage());
            }
        }
    }

    private void createNewFolder(File dir) {
        String name = JOptionPane.showInputDialog(this, "New folder name:");
        if (name != null && !name.trim().isEmpty()) {
            File newFolder = new File(dir, name);
            if (newFolder.mkdir()) {
                refreshTree();
            } else {
                JOptionPane.showMessageDialog(this, "Folder creation failed.");
            }
        }
    }

    private void addFavorite(File file) {
        if (!favoriteFiles.contains(file)) favoriteFiles.add(file);
        JOptionPane.showMessageDialog(this, "Added to favorites.");
    }

    private void showFileHistory(File file) {
        Long lastModified = fileHistory.get(file);
        String msg = lastModified != null ? "Last modified: " + new Date(lastModified) : "No history.";
        JOptionPane.showMessageDialog(this, msg);
    }

    private void refreshTree() {
        treeModel.setRoot(createTreeNodes(rootDir));
        treeModel.reload();
    }

    // Multi-project/workspace support
    public void addWorkspaceRoot(File workspaceRoot) {
        workspaceRoots.add(workspaceRoot);
        refreshTree();
    }

    // File type icons
    private Icon getFileIcon(File file) {
        String name = file.getName().toLowerCase();
        if (file.isDirectory()) return UIManager.getIcon("FileView.directoryIcon");
        if (name.endsWith(".java")) return UIManager.getIcon("FileView.fileIcon");
        if (name.endsWith(".md")) return UIManager.getIcon("FileView.fileIcon");
        if (name.endsWith(".txt")) return UIManager.getIcon("FileView.fileIcon");
        if (name.endsWith(".xml")) return UIManager.getIcon("FileView.fileIcon");
        // ... add more types
        return UIManager.getIcon("FileView.fileIcon");
    }

    // Async file loading
    private void loadFilesAsync(File dir) {
        SwingWorker<Void, File> worker = new SwingWorker<>() {
            protected Void doInBackground() {
                File[] files = dir.listFiles();
                if (files != null) {
                    for (File f : files) publish(f);
                }
                return null;
            }
            protected void process(List<File> chunks) {
                // Optionally update UI
            }
        };
        worker.execute();
    }

    // Listen for external file changes
    private void watchFileChanges(File file) {
        try {
            WatchService watcher = FileSystems.getDefault().newWatchService();
            Path path = file.toPath().getParent();
            path.register(watcher, StandardWatchEventKinds.ENTRY_MODIFY);
            new Thread(() -> {
                try {
                    while (true) {
                        WatchKey key = watcher.take();
                        for (WatchEvent<?> event : key.pollEvents()) {
                            if (event.context().toString().equals(file.getName())) {
                                SwingUtilities.invokeLater(() -> refreshTree());
                            }
                        }
                        key.reset();
                    }
                } catch (Exception ignored) {}
            }).start();
        } catch (IOException ignored) {}
    }

    // Customizable file views (list/tree/grid)
    public void setFileViewMode(String mode) {
        // Implement switching between tree/list/grid views
    }

    // Accessibility and keyboard navigation
    private void setupAccessibility() {
        fileTree.setFocusable(true);
        fileTree.getInputMap().put(KeyStroke.getKeyStroke("F2"), "renameFile");
        fileTree.getActionMap().put("renameFile", new AbstractAction() {
            public void actionPerformed(ActionEvent e) {
                TreePath path = fileTree.getSelectionPath();
                if (path != null) renameFile(getFileFromPath(path));
            }
        });
    }

    // Settings for hidden files, exclusions, favorites
    public void setShowHiddenFiles(boolean show) {
        // Implement hidden file filtering
    }

    public void setExclusions(List<String> patterns) {
        // Implement exclusion filtering
    }

    public void showFavorites() {
        if (favoriteFiles.isEmpty()) {
            JOptionPane.showMessageDialog(this, "No favorites.");
            return;
        }
        StringBuilder sb = new StringBuilder("Favorite files:\n");
        for (File f : favoriteFiles) sb.append(f.getAbsolutePath()).append("\n");
        JOptionPane.showMessageDialog(this, sb.toString());
    }
import javax.swing.*;
import javax.swing.tree.*;
import java.awt.*;
import java.awt.event.*;
import java.io.*;
import java.nio.file.*;
import java.util.*;

public class FileManager extends JPanel {
    private JTextArea editor;
    private JTree fileTree;
    private DefaultTreeModel treeModel;
    private JTextField searchField;
    private File rootDir;
    private List<File> recentFiles = new ArrayList<>();

    public FileManager(JTextArea editor, File rootDir) {
        this.editor = editor;
        this.rootDir = rootDir;
        setLayout(new BorderLayout());

        // File explorer tree
        DefaultMutableTreeNode rootNode = createTreeNodes(rootDir);
        treeModel = new DefaultTreeModel(rootNode);
        fileTree = new JTree(treeModel);
        fileTree.setRootVisible(true);
        fileTree.addTreeSelectionListener(e -> {
            TreePath path = e.getPath();
            File file = getFileFromPath(path);
            if (file != null && file.isFile()) {
                openFile(file);
            }
        });
        JScrollPane treeScroll = new JScrollPane(fileTree);
        treeScroll.setPreferredSize(new Dimension(250, 400));

        // Search field
        searchField = new JTextField(20);
        JButton searchBtn = new JButton("Search");
        searchBtn.addActionListener(e -> searchFiles());
        JPanel searchPanel = new JPanel();
        searchPanel.add(new JLabel("Find:"));
        searchPanel.add(searchField);
        searchPanel.add(searchBtn);

        // Recent files
        JButton recentBtn = new JButton("Recent Files");
        recentBtn.addActionListener(e -> showRecentFiles());
        searchPanel.add(recentBtn);

        // Save button
        JButton saveBtn = new JButton("Save");
        saveBtn.addActionListener(e -> saveFile());
        searchPanel.add(saveBtn);

        add(searchPanel, BorderLayout.NORTH);
        add(treeScroll, BorderLayout.WEST);
    }

    private DefaultMutableTreeNode createTreeNodes(File dir) {
        DefaultMutableTreeNode node = new DefaultMutableTreeNode(dir.getName());
        File[] files = dir.listFiles();
        if (files != null) {
            for (File f : files) {
                if (f.isDirectory()) {
                    node.add(createTreeNodes(f));
                } else {
                    node.add(new DefaultMutableTreeNode(f.getName()));
                }
            }
        }
        return node;
    }

    private File getFileFromPath(TreePath path) {
        Object[] nodes = path.getPath();
        File file = rootDir;
        for (int i = 1; i < nodes.length; i++) {
            file = new File(file, nodes[i].toString());
        }
        return file.exists() ? file : null;
    }

    private void openFile(File file) {
        try {
            String content = Files.readString(file.toPath());
            editor.setText(content);
            addRecentFile(file);
        } catch (IOException e) {
            JOptionPane.showMessageDialog(this, "Error: " + e.getMessage());
        }
    }

    private void saveFile() {
        JFileChooser chooser = new JFileChooser();
        if (chooser.showSaveDialog(this) == JFileChooser.APPROVE_OPTION) {
            try {
                Files.writeString(chooser.getSelectedFile().toPath(), editor.getText());
            } catch (IOException e) {
                JOptionPane.showMessageDialog(this, "Error: " + e.getMessage());
            }
        }
    }

    private void searchFiles() {
        String query = searchField.getText().trim().toLowerCase();
        if (query.isEmpty()) return;
        List<File> matches = new ArrayList<>();
        searchRecursive(rootDir, query, matches);
        if (matches.isEmpty()) {
            JOptionPane.showMessageDialog(this, "No files found matching: " + query);
        } else {
            StringBuilder sb = new StringBuilder("Found files:\n");
            for (File f : matches) sb.append(f.getAbsolutePath()).append("\n");
            JOptionPane.showMessageDialog(this, sb.toString());
        }
    }

    private void searchRecursive(File dir, String query, List<File> matches) {
        File[] files = dir.listFiles();
        if (files != null) {
            for (File f : files) {
                if (f.isDirectory()) {
                    searchRecursive(f, query, matches);
                } else if (f.getName().toLowerCase().contains(query)) {
                    matches.add(f);
                }
            }
        }
    }

    private void addRecentFile(File file) {
        recentFiles.remove(file);
        recentFiles.add(0, file);
        if (recentFiles.size() > 10) recentFiles.remove(10);
    }

    private void showRecentFiles() {
        if (recentFiles.isEmpty()) {
            JOptionPane.showMessageDialog(this, "No recent files.");
            return;
        }
        StringBuilder sb = new StringBuilder("Recent files:\n");
        for (File f : recentFiles) sb.append(f.getAbsolutePath()).append("\n");
        JOptionPane.showMessageDialog(this, sb.toString());
    }
}