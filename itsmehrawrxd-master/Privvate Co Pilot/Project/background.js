// In background.js
function sendNativeMessage() {
  // Connects to the host using the name defined in the manifest file
  let port = chrome.runtime.connectNative('com.my_company.my_application');

  port.onMessage.addListener((message) => {
    console.log("Received from native host:", message);
  });

  port.onDisconnect.addListener(() => {
    if (chrome.runtime.lastError) {
      console.error("Disconnected:", chrome.runtime.lastError.message);
    }
  });

  port.postMessage({ action: "sayHello" });
}

// Call this function from your popup or as a test
sendNativeMessage();
