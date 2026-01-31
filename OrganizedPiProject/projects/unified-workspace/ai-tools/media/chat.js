const vscode = acquireVsCodeApi();

const msgsEl = document.getElementById('msgs');
const txt = document.getElementById('txt');
const send = document.getElementById('send');
const btnIndex = document.getElementById('btnIndex');
const idxInfo = document.getElementById('idxInfo');

function push(role, content) {
  const div = document.createElement('div');
  div.className = `msg ${role}`;
  div.textContent = content;
  msgsEl.appendChild(div);
  msgsEl.scrollTop = msgsEl.scrollHeight;
}

send.addEventListener('click', () => {
  const content = txt.value.trim();
  if (!content) return;
  push('user', content);
  vscode.postMessage({ type: 'chat', payload: { messages: [{ role: 'user', content }] } });
  txt.value = '';
  txt.focus();
});

btnIndex.addEventListener('click', () => {
  vscode.postMessage({ type: 'indexSummary' });
});

window.addEventListener('message', (ev) => {
  const { type, payload } = ev.data || {};
  if (type === 'chatResponse') push('assistant', payload);
  if (type === 'indexSummaryResponse') {
    const text = payload?.files ? `${payload.files} files indexed` : 'No index yet';
    idxInfo.textContent = text;
  }
});