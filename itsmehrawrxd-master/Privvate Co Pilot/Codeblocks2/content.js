// Injected into the current tab to handle file transfers and page content
chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
  if (request.action === 'performAction') {
    // Existing logic for mouse/keyboard control
    // You would parse the AI response here for file-related instructions
  }
});

// Triggered by the popup
function selectAndSendFile() {
  const fileInput = document.createElement('input');
  fileInput.type = 'file';
  fileInput.addEventListener('change', (event) => {
    const file = event.target.files[0];
    if (file) {
      const reader = new FileReader();
      reader.onload = (e) => {
        // Encrypt the file content before sending
        const encryptedData = e.target.result; // Placeholder for real encryption
        chrome.runtime.sendMessage({ action: 'sendFile', fileData: encryptedData });
      };
      reader.readAsDataURL(file); // Or read as ArrayBuffer for binary files
    }
  });
  fileInput.click();
}
