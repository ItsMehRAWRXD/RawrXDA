import javax.swing.*;
import java.awt.*;

public class TodoDemo {
    public static void main(String[] args) {
        SwingUtilities.invokeLater(() -> {
            JFrame frame = new JFrame("Visual TODO Demo");
            frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
            frame.setSize(600, 500);
            frame.setLocationRelativeTo(null);
            
            TodoPanel todoPanel = new TodoPanel();
            frame.add(todoPanel, BorderLayout.CENTER);
            
            frame.setVisible(true);
        });
    }
}