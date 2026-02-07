import javax.swing.*;
import java.awt.*;
import java.awt.event.ActionEvent;
import java.io.File;

public class GradlePanel extends JPanel {
    private final GradleManager gradleManager;
    private final JTextArea outputArea;
    private final JComboBox<String> taskBox;

    public GradlePanel(File projectDir) {
        this.gradleManager = new GradleManager(projectDir);
        setLayout(new BorderLayout());

        outputArea = new JTextArea(15, 60);
        outputArea.setEditable(false);
        add(new JScrollPane(outputArea), BorderLayout.CENTER);

        JPanel topPanel = new JPanel(new FlowLayout());
        taskBox = new JComboBox<>(new String[]{"build", "clean", "test"});
        JButton runButton = new JButton("Run Task");
        runButton.addActionListener(this::runTask);
        topPanel.add(new JLabel("Gradle Task:"));
        topPanel.add(taskBox);
        topPanel.add(runButton);
        add(topPanel, BorderLayout.NORTH);
    }

    private void runTask(ActionEvent e) {
        String task = (String) taskBox.getSelectedItem();
        try {
            outputArea.append("Running: " + task + "\n");
            switch (task) {
                case "build" -> gradleManager.build();
                case "clean" -> gradleManager.clean();
                case "test" -> gradleManager.test();
            }
            outputArea.append("Task completed: " + task + "\n");
        } catch (Exception ex) {
            outputArea.append("Error: " + ex.getMessage() + "\n");
        }
    }

    public void close() {
        gradleManager.close();
    }
}
