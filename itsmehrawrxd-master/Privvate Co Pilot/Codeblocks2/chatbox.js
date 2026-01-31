document.getElementById('openChat').addEventListener('click', () => {
  chrome.sidePanel.open();
});

document.getElementById('sendFile').addEventListener('click', () => {
  // Logic to send a selected file to the AI or another tab
  document.getElementById('status').textContent = 'File transfer initiated...';
  // See `content.js` and `background.js` for implementation
});
