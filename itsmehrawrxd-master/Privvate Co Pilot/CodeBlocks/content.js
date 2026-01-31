chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
  if (request.action === 'performAction') {
    // This is where you would parse the AI's response and translate it into
    // JavaScript commands for mouse and keyboard actions.
    // For security, it's CRITICAL to have a safe mapping of AI instructions
    // to pre-defined, non-destructive actions.
    // Example: "click the button with ID 'submit'" -> document.getElementById('submit').click();
    // Do NOT use eval() as it can execute malicious code.

    const action = request.script;
    console.log("AI command to execute:", action);

    // Placeholder for safe execution logic.
    // Example for "Type 'Hello, World!'"
    if (action.includes("Type 'Hello, World!'")) {
      const inputField = document.querySelector('input, textarea'); // Find a suitable input field
      if (inputField) {
        inputField.focus();
        inputField.value = 'Hello, World!';
      }
    }
  }
});
