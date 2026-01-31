document.getElementById('executeBtn').addEventListener('click', () => {
  const command = document.getElementById('commandInput').value;
  if (command) {
    chrome.runtime.sendMessage({ action: 'executeCommand', command: command });
    document.getElementById('commandInput').value = '';
    document.getElementById('response').textContent = 'Processing command...';
  }
});

chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
  if (request.action === 'displayResponse') {
    document.getElementById('response').textContent = request.response;
  }
});
