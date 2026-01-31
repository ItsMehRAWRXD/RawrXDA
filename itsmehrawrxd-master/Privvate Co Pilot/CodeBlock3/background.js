// Private AI Co Pilot - Background Script
// This file runs in the background and acts as the extension's central hub.

// Use the generic 'browser' namespace for cross-browser compatibility.
// In Chrome, 'browser' is an alias for 'chrome'.
browser.runtime.onMessage.addListener((request, sender, sendResponse) => {
  // Handle chat messages with Google Gemini
  if (request.action === 'chatMessage') {
    handleGeminiRequest(request.message, sendResponse);
    return true; // Indicate async response
  }
  
  // Handle API key storage
  if (request.action === 'saveApiKey') {
    browser.storage.local.set({ geminiApiKey: request.apiKey });
    sendResponse({ success: true });
    return true;
  }
  
  // Handle API key retrieval
  if (request.action === 'getApiKey') {
    browser.storage.local.get(['geminiApiKey']).then(result => {
      sendResponse({ apiKey: result.geminiApiKey || null });
    });
    return true;
  }
});

// Function to handle Google Gemini API requests
async function handleGeminiRequest(message, sendResponse) {
  try {
    // Get API key from storage
    const result = await browser.storage.local.get(['geminiApiKey']);
    const apiKey = result.geminiApiKey;
    
    if (!apiKey) {
      sendResponse({
        action: 'aiResponse',
        response: 'Please configure your Google Gemini API key in the extension settings.',
        error: 'No API key configured'
      });
      return;
    }

    // Google Gemini API endpoint
    const geminiApiUrl = `https://generativelanguage.googleapis.com/v1beta/models/gemini-pro:generateContent?key=${apiKey}`;

    const response = await fetch(geminiApiUrl, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({
        contents: [{
          parts: [{
            text: message
          }]
        }],
        generationConfig: {
          temperature: 0.7,
          topK: 40,
          topP: 0.95,
          maxOutputTokens: 1024,
        }
      })
    });

    if (!response.ok) {
      throw new Error(`API request failed: ${response.status} ${response.statusText}`);
    }

    const data = await response.json();
    
    if (data.candidates && data.candidates[0] && data.candidates[0].content) {
      const aiResponse = data.candidates[0].content.parts[0].text;
      sendResponse({
        action: 'aiResponse',
        response: aiResponse
      });
    } else {
      throw new Error('Invalid response format from Gemini API');
    }

  } catch (error) {
    console.error('Error calling Gemini API:', error);
    sendResponse({
      action: 'aiResponse',
      response: `Sorry, an error occurred: ${error.message}`,
      error: error.message
    });
  }
}
