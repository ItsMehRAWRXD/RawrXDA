// background.js (Updated for custom Ollama service)
// Use the generic 'chrome' namespace for Chrome extension compatibility.

// Add context menu item on install
chrome.runtime.onInstalled.addListener(() => {
  chrome.contextMenus.create({
    id: "explainWithAi",
    title: " Explain with AI",
    contexts: ["selection"]
  });
  
  console.log('AI Co Pilot extension installed');
});

// Add listener for context menu item clicks
chrome.contextMenus.onClicked.addListener(async (info, tab) => {
  if (info.menuItemId === "explainWithAi" && info.selectionText) {
    const selectedText = info.selectionText.trim();
    if (selectedText.length < 3) {
      return; // Don't process very short selections
    }
    
    const { apiUrl, apiKey } = await chrome.storage.local.get(['apiUrl', 'apiKey']);
    if (!apiUrl) {
      chrome.tabs.sendMessage(tab.id, { 
        action: 'showError', 
        message: 'Please set the API URL in the extension settings.' 
      });
      return;
    }
    
    handleOllamaRequest(`Explain this: "${selectedText}"`, null, tab.id, apiUrl, apiKey);
  }
});

chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
  // Handle chat messages from the popup
  if (request.action === 'chatMessage') {
    handleOllamaRequest(request.message, sendResponse);
    return true; // Indicate async response
  }
  
  // Handle API URL and Key storage
  if (request.action === 'saveSettings') {
    chrome.storage.local.set({ apiUrl: request.apiUrl, apiKey: request.apiKey });
    sendResponse({ success: true });
    return true;
  }
  
  // Handle selected text requests
  if (request.action === 'getSelectedText') {
    chrome.tabs.query({ active: true, currentWindow: true }, (tabs) => {
      chrome.tabs.sendMessage(tabs[0].id, { action: 'getSelectedText' }, (response) => {
        sendResponse(response);
      });
    });
    return true;
  }
});

async function getSettings() {
  return await chrome.storage.local.get(['apiUrl', 'apiKey']);
}

// Function to handle Ollama API requests
async function handleOllamaRequest(message, sendResponse, tabId = null) {
  const { apiUrl, apiKey } = await getSettings();

  if (!apiUrl) {
    const errorResponse = {
      action: 'aiResponse',
      response: 'Please configure your Ollama server URL in the extension settings.',
      error: 'No API URL configured'
    };
    if (tabId) {
      chrome.tabs.sendMessage(tabId, errorResponse);
    } else if (sendResponse) {
      sendResponse(errorResponse);
    }
    return;
  }
  
  try {
    const ollamaEndpoint = `${apiUrl}/api/generate`;
    
    const headers = {
      'Content-Type': 'application/json',
    };
    if (apiKey) {
      headers['Authorization'] = `Bearer ${apiKey}`;
    }
    
    const response = await fetch(ollamaEndpoint, {
      method: 'POST',
      headers: headers,
      body: JSON.stringify({
        model: "llama3", // Specify the model you downloaded to the server
        prompt: message,
        stream: false // Set to true for streaming, requires server-side support
      })
    });

    if (!response.ok) {
      throw new Error(`Ollama API request failed: ${response.status} ${response.statusText}`);
    }

    const data = await response.json();
    
    if (data.response) {
      const aiResponse = data.response;
      if (tabId) {
        chrome.tabs.sendMessage(tabId, { action: 'showAiExplanation', explanation: aiResponse });
      } else if (sendResponse) {
        sendResponse({
          action: 'aiResponse',
          response: aiResponse
        });
      }
    } else {
      throw new Error('Invalid response format from Ollama API');
    }

  } catch (error) {
    console.error('Error calling Ollama API:', error);
    const errorResponse = {
      action: 'aiResponse',
      response: `Sorry, an error occurred: ${error.message}`,
      error: error.message
    };
    if (tabId) {
      chrome.tabs.sendMessage(tabId, errorResponse);
    } else if (sendResponse) {
      sendResponse(errorResponse);
    }
  }
}

// Utility function for authenticated requests (keeping for compatibility)
async function sendAuthenticatedRequest(endpoint, method, data) {
  const token = await chrome.storage.local.get(['jwtToken']);
  if (!token.jwtToken) {
    throw new Error('User not authenticated.');
  }

  const response = await fetch(`https://your-api-server.com/api/${endpoint}`, {
    method: method,
    headers: {
      'Content-Type': 'application/json',
      'Authorization': `Bearer ${token.jwtToken}`
    },
    body: JSON.stringify(data)
  });

  if (!response.ok) {
    throw new Error(`API call failed: ${response.statusText}`);
  }
  return await response.json();
}
