// popup.js
document.addEventListener('DOMContentLoaded', () => {
  const chatMessageInput = document.getElementById('chatMessage');
  const sendButton = document.getElementById('sendButton');
  const responseDiv = document.getElementById('response');
  const apiUrlInput = document.getElementById('apiUrlInput');
  const apiKeyInput = document.getElementById('apiKeyInput');
  const saveSettingsButton = document.getElementById('saveSettingsButton');
  const statusDiv = document.getElementById('status');
  const loadingDiv = document.getElementById('loading');

  // Load settings on startup
  chrome.storage.local.get(['apiUrl', 'apiKey']).then(result => {
    if (result.apiUrl) {
      apiUrlInput.value = result.apiUrl;
    }
    if (result.apiKey) {
      apiKeyInput.value = result.apiKey;
    }
    if (result.apiUrl || result.apiKey) {
      statusDiv.textContent = 'Settings loaded';
    }
  });

  // Handle saving settings
  saveSettingsButton.addEventListener('click', async () => {
    const apiUrl = apiUrlInput.value.trim();
    const apiKey = apiKeyInput.value.trim();
    
    if (!apiUrl) {
      statusDiv.textContent = 'Please enter an API server URL';
      statusDiv.style.color = '#f44336';
      return;
    }

    try {
      await chrome.storage.local.set({ apiUrl: apiUrl, apiKey: apiKey });
      statusDiv.textContent = 'Settings saved successfully!';
      statusDiv.style.color = '#4CAF50';
      
      // Clear status after 3 seconds
      setTimeout(() => {
        statusDiv.textContent = '';
      }, 3000);
    } catch (error) {
      statusDiv.textContent = 'Error saving settings';
      statusDiv.style.color = '#f44336';
    }
  });

  // Handle sending chat message
  sendButton.addEventListener('click', async () => {
    const message = chatMessageInput.value.trim();
    if (!message) {
      responseDiv.textContent = 'Please enter a message';
      return;
    }

    // Check if API URL is configured
    const result = await chrome.storage.local.get(['apiUrl', 'apiKey']);
    if (!result.apiUrl) {
      responseDiv.textContent = 'Please configure your API server URL first';
      return;
    }

    // Show loading state
    loadingDiv.style.display = 'block';
    responseDiv.textContent = '';
    sendButton.disabled = true;
    sendButton.textContent = 'Sending...';

    // Set up streaming response handler
    const responseStreamHandler = (response) => {
      if (response && response.action === 'aiResponseChunk') {
        if (response.done) {
          console.log('Stream finished.');
          chrome.runtime.onMessage.removeListener(responseStreamHandler);
          // Hide loading state
          loadingDiv.style.display = 'none';
          sendButton.disabled = false;
          sendButton.textContent = 'Send Message';
        } else if (response.chunk) {
          if (responseDiv.textContent === 'Thinking...') {
            responseDiv.textContent = '';
          }
          responseDiv.textContent += response.chunk;
          responseDiv.style.color = 'white';
        }
      } else if (response && response.action === 'aiResponse') {
        if (response.response) {
          responseDiv.textContent = response.response;
          responseDiv.style.color = 'white';
        } else if (response.error) {
          responseDiv.textContent = `Error: ${response.error}`;
          responseDiv.style.color = '#ffcccb';
        }
        // Hide loading state
        loadingDiv.style.display = 'none';
        sendButton.disabled = false;
        sendButton.textContent = 'Send Message';
        chrome.runtime.onMessage.removeListener(responseStreamHandler);
      }
    };

    chrome.runtime.onMessage.addListener(responseStreamHandler);

    try {
      chrome.runtime.sendMessage({ 
        action: 'chatMessage', 
        message: message 
      });
    } catch (error) {
      responseDiv.textContent = `Error: ${error.message}`;
      responseDiv.style.color = '#ffcccb';
      // Hide loading state
      loadingDiv.style.display = 'none';
      sendButton.disabled = false;
      sendButton.textContent = 'Send Message';
      chrome.runtime.onMessage.removeListener(responseStreamHandler);
    }
  });

  // Handle Enter key in textarea
  chatMessageInput.addEventListener('keydown', (e) => {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault();
      sendButton.click();
    }
  });

  // Auto-resize textarea
  chatMessageInput.addEventListener('input', () => {
    chatMessageInput.style.height = 'auto';
    chatMessageInput.style.height = Math.min(chatMessageInput.scrollHeight, 150) + 'px';
  });

  // Clear response when typing new message
  chatMessageInput.addEventListener('input', () => {
    if (responseDiv.textContent && !responseDiv.textContent.includes('Error')) {
      responseDiv.textContent = '';
    }
  });

  // Add some helpful placeholder text
  const placeholders = [
    "Explain this code: ",
    "What does this function do? ",
    "How can I optimize this? ",
    "Debug this issue: ",
    "Write a test for: ",
    "Refactor this code: "
  ];

  let placeholderIndex = 0;
  setInterval(() => {
    if (!chatMessageInput.value && document.activeElement !== chatMessageInput) {
      placeholderIndex = (placeholderIndex + 1) % placeholders.length;
      chatMessageInput.placeholder = placeholders[placeholderIndex];
    }
  }, 3000);
});
