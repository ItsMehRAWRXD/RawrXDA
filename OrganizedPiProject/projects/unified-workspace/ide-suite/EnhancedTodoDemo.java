import javax.swing.*;
import java.awt.*;

public class EnhancedTodoDemo {
    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> {
            JFrame frame = new JFrame("Enhanced TODO System - Copilot Ready");
            frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
            frame.setSize(800, 600);
            frame.setLocationRelativeTo(null);
            
            EnhancedTodoSystem todoSystem = new EnhancedTodoSystem();
            frame.add(todoSystem, BorderLayout.CENTER);
            
            // Add menu bar with API info
            JMenuBar menuBar = new JMenuBar();
            JMenu apiMenu = new JMenu("API");
            JMenuItem infoItem = new JMenuItem("API Info");
            infoItem.addActionListener(e -> {
                String info = "TODO API Running on http://localhost:8080\n\n" +
                             "Copilot Integration:\n" +
                             "• GET /todos - Fetch all todos\n" +
                             "• POST /todos/add - Add new todo\n" +
                             "• GET /activity - Get productivity metrics\n\n" +
                             "Example curl commands:\n" +
                             "curl http://localhost:8080/todos\n" +
                             "curl -X POST -d 'text=Fix bug&priority=HIGH' http://localhost:8080/todos/add";
                JOptionPane.showMessageDialog(frame, info, "API Information", JOptionPane.INFORMATION_MESSAGE);
            });
            apiMenu.add(infoItem);
            menuBar.add(apiMenu);
            frame.setJMenuBar(menuBar);
            
            frame.setVisible(true);
        });
    }
}