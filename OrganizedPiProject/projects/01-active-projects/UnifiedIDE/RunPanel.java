import javax.swing.*;
import java.awt.*;

public class RunPanel extends JPanel {
    private JTextArea outputArea;
    private JComboBox<String> scriptBox, debuggerBox, compilerBox;
    private JButton runScriptButton, runDebuggerButton, runCompilerButton;

    public RunPanel() {
    setLayout(new BorderLayout());
    outputArea = new JTextArea();
    outputArea.setEditable(false);
    outputArea.setFont(new Font("Monospaced", Font.PLAIN, 13));
    JScrollPane scrollPane = new JScrollPane(outputArea);
    add(scrollPane, BorderLayout.CENTER);

    scriptBox = new JComboBox<>(new String[] {"Script1", "Script2", "Script3"});
    debuggerBox = new JComboBox<>(new String[] {"DebuggerA", "DebuggerB"});
    compilerBox = new JComboBox<>(new String[] {"CompilerX", "CompilerY"});

    runScriptButton = new JButton("Run Script");
    runDebuggerButton = new JButton("Run Debugger");
    runCompilerButton = new JButton("Run Compiler");

    runScriptButton.addActionListener(e -> runScript((String)scriptBox.getSelectedItem()));
    runDebuggerButton.addActionListener(e -> runDebugger((String)debuggerBox.getSelectedItem()));
    runCompilerButton.addActionListener(e -> runCompiler((String)compilerBox.getSelectedItem()));

    JPanel controlPanel = new JPanel();
    controlPanel.add(new JLabel("Script:"));
    controlPanel.add(scriptBox);
    controlPanel.add(runScriptButton);
    controlPanel.add(new JLabel("Debugger:"));
    controlPanel.add(debuggerBox);
    controlPanel.add(runDebuggerButton);
    controlPanel.add(new JLabel("Compiler:"));
    controlPanel.add(compilerBox);
    controlPanel.add(runCompilerButton);
    add(controlPanel, BorderLayout.SOUTH);
    }

    private void runScript() {
    private void runScript(String scriptName) {
        outputArea.append("[Run Script] " + scriptName + " executed dynamically.\n");
        // Integrate with real script runner if needed
    }

    private void runDebugger() {
    private void runDebugger(String debuggerName) {
        outputArea.append("[Run Debugger] " + debuggerName + " started dynamically.\n");
        // Integrate with real debugger if needed
    }

    private void runCompiler() {
    private void runCompiler(String compilerName) {
        outputArea.append("[Run Compiler] " + compilerName + " executed dynamically.\n");
        // Integrate with real compiler if needed
    }
}
