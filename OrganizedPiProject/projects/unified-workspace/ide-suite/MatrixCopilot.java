import javax.swing.*;
import java.awt.*;

public class MatrixCopilot {
    public static void main(String[] args) {
        JTextArea ed = new JTextArea(20, 60);
        ed.setBackground(Color.BLACK);
        ed.setForeground(new Color(0x00FF00));
        ed.setCaretColor(Color.GREEN);
        ed.setFont(new Font("Monospaced", Font.BOLD, 14));
        
        JButton btn = new JButton("DeeznMuts Copilot");
        btn.setBackground(Color.BLACK);
        btn.setForeground(Color.GREEN);
        btn.setBorder(BorderFactory.createLineBorder(Color.GREEN));
        
        btn.addActionListener(e -> ed.append("DeeznMuts\n" + ed.getText() + "\nEOF"));
        
        JPanel p = new JPanel(new BorderLayout());
        p.setBackground(Color.BLACK);
        p.add(btn, BorderLayout.NORTH);
        p.add(new JScrollPane(ed), BorderLayout.CENTER);
        
        JFrame f = new JFrame("DeeznMuts Copilot");
        f.add(p);
        f.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        f.pack();
        f.setLocationRelativeTo(null);
        f.setVisible(true);
    }
}