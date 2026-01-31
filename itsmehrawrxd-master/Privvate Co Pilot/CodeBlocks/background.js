chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
  if (request.action === 'executeCommand') {
    // In a real-world scenario, you would fetch the API key from
    // `chrome.storage.local` and use it to make a request to the ChatGPT API.
    // The AI's response would then be sent to the active tab via the content script.
    // For this example, we'll simulate a response.

    const aiResponse = `Simulated action: "Type 'Hello, World!' in the active tab"`;

    chrome.tabs.query({ active: true, currentWindow: true }, (tabs) => {
      if (tabs[0]) {
        chrome.tabs.sendMessage(tabs[0].id, {
          action: 'performAction',
          script: aiResponse,
        });
      }
    });
  }
});
