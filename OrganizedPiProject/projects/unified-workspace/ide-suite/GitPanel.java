import javax.swing.*;
import java.awt.*;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.util.List;
import org.eclipse.jgit.api.errors.GitAPIException;

public class GitPanel extends JPanel {
	private final GitManager gitManager;
	private final JTextArea statusArea;
	private final JComboBox<String> branchBox;
	private final JTextField commitMessageField;

	public GitPanel(GitManager gitManager) {
		this.gitManager = gitManager;
		setLayout(new BorderLayout());

		statusArea = new JTextArea(10, 40);
		statusArea.setEditable(false);
		add(new JScrollPane(statusArea), BorderLayout.CENTER);

		JPanel bottomPanel = new JPanel(new FlowLayout());
		commitMessageField = new JTextField(20);
		JButton commitButton = new JButton("Commit");
		commitButton.addActionListener(e -> commit());
		JButton pushButton = new JButton("Push");
		pushButton.addActionListener(e -> push());
		JButton pullButton = new JButton("Pull");
		pullButton.addActionListener(e -> pull());

		branchBox = new JComboBox<>();
		JButton checkoutButton = new JButton("Checkout");
		checkoutButton.addActionListener(e -> checkoutBranch());

		bottomPanel.add(new JLabel("Commit Message:"));
		bottomPanel.add(commitMessageField);
		bottomPanel.add(commitButton);
		bottomPanel.add(pushButton);
		bottomPanel.add(pullButton);
		bottomPanel.add(new JLabel("Branch:"));
		bottomPanel.add(branchBox);
		bottomPanel.add(checkoutButton);

		add(bottomPanel, BorderLayout.SOUTH);

		refreshStatus();
		refreshBranches();
	}

	private void refreshStatus() {
		try {
			statusArea.setText(gitManager.getStatus());
		} catch (GitAPIException e) {
			statusArea.setText("Error: " + e.getMessage());
		}
	}

	private void refreshBranches() {
		try {
			branchBox.removeAllItems();
			List<String> branches = gitManager.listBranches();
			for (String branch : branches) branchBox.addItem(branch);
		} catch (GitAPIException e) {
			branchBox.addItem("Error: " + e.getMessage());
		}
	}

	private void commit() {
		try {
			gitManager.commit(commitMessageField.getText());
			refreshStatus();
		} catch (GitAPIException e) {
			JOptionPane.showMessageDialog(this, "Commit failed: " + e.getMessage());
		}
	}

	private void push() {
		try {
			gitManager.push();
			refreshStatus();
		} catch (GitAPIException e) {
			JOptionPane.showMessageDialog(this, "Push failed: " + e.getMessage());
		}
	}

	private void pull() {
		try {
			gitManager.pull();
			refreshStatus();
		} catch (GitAPIException e) {
			JOptionPane.showMessageDialog(this, "Pull failed: " + e.getMessage());
		}
	}

	private void checkoutBranch() {
		String branch = (String) branchBox.getSelectedItem();
		if (branch != null) {
			try {
				gitManager.checkoutBranch(branch);
				refreshStatus();
			} catch (GitAPIException e) {
				JOptionPane.showMessageDialog(this, "Checkout failed: " + e.getMessage());
			}
		}
	}
}
