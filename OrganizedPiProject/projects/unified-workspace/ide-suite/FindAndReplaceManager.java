import javax.swing.*;
import java.awt.*;
import java.awt.event.ActionEvent;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class FindAndReplaceManager {

    private JTextPane textPane;
    private FindDialog findDialog;
    private String lastFind = "";
    private Pattern findPattern;

    public FindAndReplaceManager(JTextPane textPane) {
        this.textPane = textPane;
        this.findDialog = new FindDialog();
    }

    public void showFindDialog() {
        findDialog.setVisible(true);
    }

    private void findNext() {
        String findText = findDialog.getFindText();
        if (findText == null || findText.isEmpty()) {
            return;
        }

        if (!findText.equals(lastFind) || findPattern == null) {
            lastFind = findText;
            int flags = 0;
            if (!findDialog.isCaseSensitive()) {
                flags |= Pattern.CASE_INSENSITIVE;
            }
            if (findDialog.isRegex()) {
                // Use the text as regex
            } else if (findDialog.isWholeWord()) {
                findText = "\\b" + findText + "\\b";
            }
            findPattern = Pattern.compile(findText, flags);
        }

        Matcher matcher = findPattern.matcher(textPane.getText());
        int start = textPane.getSelectionEnd();
        if (textPane.getSelectedText() != null && textPane.getSelectionStart() != textPane.getSelectionEnd()) {
            start = textPane.getSelectionStart() + 1;
        }


        if (matcher.find(start)) {
            textPane.select(matcher.start(), matcher.end());
        } else {
            // Wrap search
            if (matcher.find(0)) {
                textPane.select(matcher.start(), matcher.end());
            } else {
                JOptionPane.showMessageDialog(textPane, "No more occurrences found.");
            }
        }
    }

    private void replaceCurrent() {
        String replaceText = findDialog.getReplaceText();
        if (replaceText == null) {
            replaceText = "";
        }

        if (textPane.getSelectedText() != null) {
            textPane.replaceSelection(replaceText);
            findNext();
        }
    }

    private void replaceAll() {
        String findText = findDialog.getFindText();
        String replaceText = findDialog.getReplaceText();

        if (findText == null || findText.isEmpty() || replaceText == null) {
            return;
        }
        
        String content = textPane.getText();
        
        if (!findText.equals(lastFind) || findPattern == null) {
            lastFind = findText;
            int flags = 0;
            if (!findDialog.isCaseSensitive()) {
                flags |= Pattern.CASE_INSENSITIVE;
            }
            if (findDialog.isRegex()) {
                // Use the text as regex
            } else if (findDialog.isWholeWord()) {
                findText = "\\b" + findText + "\\b";
            }
            findPattern = Pattern.compile(findText, flags);
        }
        
        Matcher matcher = findPattern.matcher(content);
        String newContent = matcher.replaceAll(replaceText);
        
        textPane.setText(newContent);
    }


    class FindDialog extends JDialog {
        private JTextField findField;
        private JTextField replaceField;
        private JCheckBox caseSensitiveBox;
        private JCheckBox wholeWordBox;
        private JCheckBox regexBox;

        public FindDialog() {
            super((Frame) null, "Find and Replace", false);
            
            findField = new JTextField(20);
            replaceField = new JTextField(20);
            caseSensitiveBox = new JCheckBox("Case Sensitive");
            wholeWordBox = new JCheckBox("Whole Word");
            regexBox = new JCheckBox("Regex");

            JButton findNextButton = new JButton("Find Next");
            findNextButton.addActionListener((ActionEvent e) -> findNext());

            JButton replaceButton = new JButton("Replace");
            replaceButton.addActionListener((ActionEvent e) -> replaceCurrent());

            JButton replaceAllButton = new JButton("Replace All");
            replaceAllButton.addActionListener((ActionEvent e) -> replaceAll());

            JPanel panel = new JPanel(new GridBagLayout());
            GridBagConstraints c = new GridBagConstraints();
            c.insets = new Insets(5, 5, 5, 5);
            c.fill = GridBagConstraints.HORIZONTAL;

            c.gridx = 0; c.gridy = 0; panel.add(new JLabel("Find:"), c);
            c.gridx = 1; c.gridy = 0; c.gridwidth = 2; panel.add(findField, c);
            
            c.gridx = 0; c.gridy = 1; panel.add(new JLabel("Replace with:"), c);
            c.gridx = 1; c.gridy = 1; c.gridwidth = 2; panel.add(replaceField, c);

            c.gridwidth = 1;
            c.gridx = 0; c.gridy = 2; panel.add(caseSensitiveBox, c);
            c.gridx = 1; c.gridy = 2; panel.add(wholeWordBox, c);
            c.gridx = 2; c.gridy = 2; panel.add(regexBox, c);

            JPanel buttonPanel = new JPanel();
            buttonPanel.add(findNextButton);
            buttonPanel.add(replaceButton);
            buttonPanel.add(replaceAllButton);

            c.gridx = 0; c.gridy = 3; c.gridwidth = 3; panel.add(buttonPanel, c);

            add(panel);
            pack();
            setLocationRelativeTo(textPane);
        }

        public String getFindText() {
            return findField.getText();
        }

        public String getReplaceText() {
            return replaceField.getText();
        }

        public boolean isCaseSensitive() {
            return caseSensitiveBox.isSelected();
        }

        public boolean isWholeWord() {
            return wholeWordBox.isSelected();
        }

        public boolean isRegex() {
            return regexBox.isSelected();
        }
    }
}
```