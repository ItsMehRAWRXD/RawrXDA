export function renderOverlay(text) {
  const overlay = document.getElementById('overlay');
  overlay.innerText = text;
  overlay.style.color = 'lime';
  overlay.style.fontSize = '24px';
  overlay.style.fontFamily = 'monospace';
  overlay.style.padding = '20px';
}
