// This file runs in the background and acts as the extension's central hub.

// Use the generic 'browser' namespace for cross-browser compatibility.
// In Chrome, 'browser' is an alias for 'chrome'.
// For older versions of Firefox, you might need a polyfill, but it is now standard.
// Source: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Content_scripts
browser.runtime.onMessage.addListener((request, sender, sendResponse) => {
  // Check if the message is the one we want to handle.
  if (request.action === 'chatMessage') {
    // This is where you would call the external AI service.
    // Replace the URL with your chosen AI's API endpoint.
    const aiApiUrl = 'YOUR_AI_API_ENDPOINT';

    // To use an external API, you must request host permissions in your manifest.json.
    // See the manifest.json section below for details.
    fetch(aiApiUrl, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        // WARNING: NEVER HARDCODE YOUR API KEY.
        // It's recommended to retrieve it from a more secure location or a backend server.
        // For local development, this is okay, but not for distribution.
        'Authorization': 'Bearer YOUR_AI_API_KEY_HERE' 
      },
      body: JSON.stringify({
        prompt: request.message,
        // Other parameters as required by your AI API
      })
    })
    .then(response => response.json())
    .then(data => {
      // Send the AI's response back to the popup script.
      browser.runtime.sendMessage({
        action: 'aiResponse',
        response: data.choices[0].text // Adjust based on your AI API's response structure
      });
    })
    .catch(error => {
      console.error('Error calling AI API:', error);
      browser.runtime.sendMessage({
        action: 'aiResponse',
        response: 'Sorry, an error occurred while processing your request.'
      });
    });

    // Return true to indicate that you want to send a response asynchronously.
    return true; 
  }
});
