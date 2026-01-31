import javax.swing.*;
import java.awt.*;
import java.awt.event.*;
import java.net.*;
import java.io.*;
import org.json.*;

public class MarketplacePanel extends JPanel {
    private DefaultListModel<String> availableExtensions;
    private JList<String> availableList;
    private DefaultListModel<String> installedExtensions;
    private JList<String> installedList;
    private JButton refreshButton, installButton;

    public MarketplacePanel() {
        setLayout(new BorderLayout());

        JLabel title = new JLabel("Extension Marketplace", JLabel.CENTER);
        title.setFont(new Font("Segoe UI", Font.BOLD, 18));
        add(title, BorderLayout.NORTH);

        availableExtensions = new DefaultListModel<>();
        availableList = new JList<>(availableExtensions);
        installedExtensions = new DefaultListModel<>();
        installedList = new JList<>(installedExtensions);

        JPanel listsPanel = new JPanel(new GridLayout(1, 2, 10, 0));
        listsPanel.add(new JScrollPane(availableList));
        listsPanel.add(new JScrollPane(installedList));
        add(listsPanel, BorderLayout.CENTER);

        refreshButton = new JButton("Refresh");
        installButton = new JButton("Install Selected");
        JPanel buttonPanel = new JPanel();
        buttonPanel.add(refreshButton);
        buttonPanel.add(installButton);
        add(buttonPanel, BorderLayout.SOUTH);

        refreshButton.addActionListener(e -> fetchExtensions());
        installButton.addActionListener(e -> installSelectedExtension());

        fetchExtensions();
    }

    private void fetchExtensions() {
        availableExtensions.clear();
        try {
            URL url = new URL("https://example.com/extensions.json"); // Replace with your server
            HttpURLConnection conn = (HttpURLConnection) url.openConnection();
            conn.setRequestMethod("GET");
            BufferedReader reader = new BufferedReader(new InputStreamReader(conn.getInputStream()));
            StringBuilder jsonBuilder = new StringBuilder();
            String line;
            while ((line = reader.readLine()) != null) {
                jsonBuilder.append(line);
            }
            reader.close();

            JSONArray extensionsArray = new JSONArray(jsonBuilder.toString());
            for (int i = 0; i < extensionsArray.length(); i++) {
                JSONObject ext = extensionsArray.getJSONObject(i);
                String name = ext.getString("name");
                String desc = ext.getString("description");
                availableExtensions.addElement(name + " - " + desc);
            }
        } catch (Exception ex) {
            availableExtensions.addElement("Error fetching extensions: " + ex.getMessage());
        }
    }

    private void installSelectedExtension() {
        int idx = availableList.getSelectedIndex();
        if (idx < 0) return;
        String selected = availableExtensions.getElementAt(idx);
        installedExtensions.addElement(selected);
        JOptionPane.showMessageDialog(this, "Extension installed: " + selected);
        // Here you would download and load the JAR dynamically
    }
}
