// A simple state manager for the AI conversation
const conversationHistory = [];

// Handles messages from the chatbox
chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
  if (request.action === 'chatMessage') {
    conversationHistory.push({ role: 'user', content: request.message });
    // Securely call ChatGPT API with the history and send the response back
    // Use an API call similar to the previous example
    // Once AI responds, send it to the chatbox:
    // chrome.runtime.sendMessage({ action: 'aiResponse', response: 'AI response here' });
  } else if (request.action === 'sendFile') {
    // Logic to handle file data from `content.js` and pass it to Native Host
    // Encrypt file data before passing
    const fileData = request.fileData;
    // chrome.runtime.sendNativeMessage('com.my_company.my_application', { ... });
  }
});

// A function to securely get the API key from storage
async function getApiKey() {
  return (await chrome.storage.local.get(['apiKey'])).apiKey;
}
