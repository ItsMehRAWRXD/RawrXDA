import javax.swing.*;
import java.awt.*;
import java.awt.event.ActionEvent;
import java.util.ArrayList;
import java.util.List;

public class TodoPanel extends JPanel {
    private final DefaultListModel<TodoItem> listModel;
    private final JList<TodoItem> todoList;
    private final JTextField newTodoField;

    public TodoPanel() {
        setLayout(new BorderLayout());
        
        listModel = new DefaultListModel<>();
        todoList = new JList<>(listModel);
        todoList.setCellRenderer(new TodoCellRenderer());
        todoList.setSelectionMode(ListSelectionModel.SINGLE_SELECTION);
        
        JScrollPane scrollPane = new JScrollPane(todoList);
        add(scrollPane, BorderLayout.CENTER);
        
        JPanel inputPanel = new JPanel(new BorderLayout());
        newTodoField = new JTextField();
        JButton addButton = new JButton("Add");
        JButton toggleButton = new JButton("Toggle");
        JButton deleteButton = new JButton("Delete");
        
        inputPanel.add(newTodoField, BorderLayout.CENTER);
        JPanel buttonPanel = new JPanel();
        buttonPanel.add(addButton);
        buttonPanel.add(toggleButton);
        buttonPanel.add(deleteButton);
        inputPanel.add(buttonPanel, BorderLayout.EAST);
        
        add(inputPanel, BorderLayout.SOUTH);
        
        addButton.addActionListener(e -> addTodo());
        toggleButton.addActionListener(e -> toggleTodo());
        deleteButton.addActionListener(e -> deleteTodo());
        newTodoField.addActionListener(e -> addTodo());
        
        // Add initial todos
        addInitialTodos();
    }
    
    private void addTodo() {
        String text = newTodoField.getText().trim();
        if (!text.isEmpty()) {
            listModel.addElement(new TodoItem(text));
            newTodoField.setText("");
        }
    }
    
    private void toggleTodo() {
        int index = todoList.getSelectedIndex();
        if (index >= 0) {
            TodoItem item = listModel.getElementAt(index);
            item.setCompleted(!item.isCompleted());
            todoList.repaint();
        }
    }
    
    private void deleteTodo() {
        int index = todoList.getSelectedIndex();
        if (index >= 0) {
            listModel.removeElementAt(index);
        }
    }
    
    private void addInitialTodos() {
        // Missing core components
        listModel.addElement(new TodoItem("Implement CodeFoldingManager class"));
        listModel.addElement(new TodoItem("Implement MultiCursorManager class"));
        listModel.addElement(new TodoItem("Implement AutoCompleteEngine class"));
        listModel.addElement(new TodoItem("Implement LiveErrorDetector class"));
        listModel.addElement(new TodoItem("Implement CommentToggler utility"));
        listModel.addElement(new TodoItem("Implement LineManipulator utility"));
        listModel.addElement(new TodoItem("Implement CodeStructureAnalyzer"));
        listModel.addElement(new TodoItem("Implement AdvancedEditorKit"));
        
        // Missing AI integration
        listModel.addElement(new TodoItem("Fix editor reference in IDEMain"));
        listModel.addElement(new TodoItem("Connect AI providers to editor"));
        listModel.addElement(new TodoItem("Add streaming responses for AI"));
        listModel.addElement(new TodoItem("Implement proper JSON parsing"));
        
        // Missing Git features
        listModel.addElement(new TodoItem("Add git add/staging to GitManager"));
        listModel.addElement(new TodoItem("Add merge conflict detection"));
        listModel.addElement(new TodoItem("Connect GitPanel to GitManager properly"));
        
        // Missing editor features
        listModel.addElement(new TodoItem("Connect EditorIntegration methods"));
        listModel.addElement(new TodoItem("Implement undo/redo in toolbar"));
        listModel.addElement(new TodoItem("Implement find/replace dialogs"));
        listModel.addElement(new TodoItem("Add proper icon loading"));
        
        // Testing and polish
        listModel.addElement(new TodoItem("Write unit tests for all components"));
        listModel.addElement(new TodoItem("Add error handling throughout"));
        listModel.addElement(new TodoItem("Performance optimization"));
        listModel.addElement(new TodoItem("Add comprehensive documentation"));
    }
    
    private static class TodoItem {
        private String text;
        private boolean completed;
        
        public TodoItem(String text) {
            this.text = text;
            this.completed = false;
        }
        
        public String getText() { return text; }
        public boolean isCompleted() { return completed; }
        public void setCompleted(boolean completed) { this.completed = completed; }
        
        @Override
        public String toString() {
            return (completed ? "? " : "? ") + text;
        }
    }
    
    private static class TodoCellRenderer extends DefaultListCellRenderer {
        @Override
        public Component getListCellRendererComponent(JList<?> list, Object value, int index,
                boolean isSelected, boolean cellHasFocus) {
            super.getListCellRendererComponent(list, value, index, isSelected, cellHasFocus);
            
            if (value instanceof TodoItem) {
                TodoItem item = (TodoItem) value;
                setText(item.toString());
                setForeground(item.isCompleted() ? Color.GRAY : Color.BLACK);
                if (item.isCompleted()) {
                    setFont(getFont().deriveFont(Font.ITALIC));
                }
            }
            return this;
        }
    }
}