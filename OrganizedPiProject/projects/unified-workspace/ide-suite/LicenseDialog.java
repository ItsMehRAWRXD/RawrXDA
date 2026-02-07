import javax.swing.*;
import java.awt.*;
import java.awt.event.*;

public class LicenseDialog extends JDialog {
    private PaidTierManager tierManager;
    private JTextField licenseField;
    private JComboBox<String> tierCombo;
    private boolean activated = false;
    
    public LicenseDialog(Frame parent, PaidTierManager tierManager) {
        super(parent, "π-Engine License Activation", true);
        this.tierManager = tierManager;
        setupUI();
    }
    
    private void setupUI() {
        setLayout(new BorderLayout());
        
        JPanel mainPanel = new JPanel(new GridBagLayout());
        GridBagConstraints gbc = new GridBagConstraints();
        
        // Title
        JLabel titleLabel = new JLabel("Activate π-Engine Pro Features");
        titleLabel.setFont(new Font(Font.SANS_SERIF, Font.BOLD, 16));
        gbc.gridx = 0; gbc.gridy = 0; gbc.gridwidth = 2; gbc.insets = new Insets(10, 10, 20, 10);
        mainPanel.add(titleLabel, gbc);
        
        // Current tier
        JLabel currentLabel = new JLabel("Current Tier: " + tierManager.getCurrentTier());
        gbc.gridy = 1; gbc.gridwidth = 2;
        mainPanel.add(currentLabel, gbc);
        
        // Tier selection
        gbc.gridy = 2; gbc.gridwidth = 1; gbc.anchor = GridBagConstraints.WEST;
        mainPanel.add(new JLabel("Select Tier:"), gbc);
        
        tierCombo = new JComboBox<>(new String[]{"PRO", "ENTERPRISE"});
        gbc.gridx = 1; gbc.fill = GridBagConstraints.HORIZONTAL;
        mainPanel.add(tierCombo, gbc);
        
        // License key
        gbc.gridx = 0; gbc.gridy = 3; gbc.fill = GridBagConstraints.NONE;
        mainPanel.add(new JLabel("License Key:"), gbc);
        
        licenseField = new JTextField(30);
        gbc.gridx = 1; gbc.fill = GridBagConstraints.HORIZONTAL;
        mainPanel.add(licenseField, gbc);
        
        // Generate button for demo
        JButton generateBtn = new JButton("Generate Demo Key");
        generateBtn.addActionListener(e -> {
            String tier = (String) tierCombo.getSelectedItem();
            String key = "PIENGINE-" + tier + "-" + tier.hashCode();
            licenseField.setText(key);
        });
        gbc.gridy = 4; gbc.gridwidth = 2; gbc.fill = GridBagConstraints.NONE;
        mainPanel.add(generateBtn, gbc);
        
        // Features info
        JTextArea featuresArea = new JTextArea(8, 40);
        featuresArea.setEditable(false);
        featuresArea.setText(getFeaturesText());
        featuresArea.setBackground(getBackground());
        JScrollPane scrollPane = new JScrollPane(featuresArea);
        gbc.gridy = 5; gbc.fill = GridBagConstraints.BOTH; gbc.weightx = 1; gbc.weighty = 1;
        mainPanel.add(scrollPane, gbc);
        
        add(mainPanel, BorderLayout.CENTER);
        
        // Buttons
        JPanel buttonPanel = new JPanel(new FlowLayout());
        JButton activateBtn = new JButton("Activate");
        JButton cancelBtn = new JButton("Cancel");
        
        activateBtn.addActionListener(e -> activate());
        cancelBtn.addActionListener(e -> dispose());
        
        buttonPanel.add(activateBtn);
        buttonPanel.add(cancelBtn);
        add(buttonPanel, BorderLayout.SOUTH);
        
        setSize(500, 400);
        setLocationRelativeTo(getParent());
    }
    
    private String getFeaturesText() {
        return "FREE TIER:\n" +
               "• 1 project maximum\n" +
               "• 10 compilations per hour\n" +
               "• Basic language support\n\n" +
               
               "PRO TIER:\n" +
               "• 5 projects maximum\n" +
               "• 100 compilations per hour\n" +
               "• Multi-compiler support\n" +
               "• Cloud synchronization\n\n" +
               
               "ENTERPRISE TIER:\n" +
               "• Unlimited projects\n" +
               "• Unlimited compilations\n" +
               "• All advanced tools\n" +
               "• Priority support\n" +
               "• Custom integrations";
    }
    
    private void activate() {
        String tier = (String) tierCombo.getSelectedItem();
        String key = licenseField.getText().trim();
        
        if (key.isEmpty()) {
            JOptionPane.showMessageDialog(this, "Please enter a license key");
            return;
        }
        
        if (tierManager.setPaidTier(tier, key)) {
            activated = true;
            JOptionPane.showMessageDialog(this, "License activated successfully!\nTier: " + tier);
            dispose();
        } else {
            JOptionPane.showMessageDialog(this, "Invalid license key for " + tier + " tier");
        }
    }
    
    public boolean isActivated() {
        return activated;
    }
}