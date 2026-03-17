const { ipcRenderer } = require('electron');

// Connect to Python WebSocket server
const ws = new WebSocket('ws://localhost:8081');

ws.onopen = () => {
    console.log('Connected to Python backend');
    document.getElementById('status').innerText = 'Connected';
};

ws.onmessage = (event) => {
    const data = JSON.parse(event.data);
    handlePythonMessage(data);
};

ws.onerror = (error) => {
    console.error('WebSocket error:', error);
    document.getElementById('status').innerText = 'Error';
};

ws.onclose = () => {
    console.log('Disconnected from Python backend');
    document.getElementById('status').innerText = 'Disconnected';
};

// Handle messages from main process
ipcRenderer.on('python-output', (event, data) => {
    console.log('Python output:', data);
    // Handle Python output in the UI
});

ipcRenderer.on('python-error', (event, data) => {
    console.error('Python error:', data);
    // Handle Python errors in the UI
});

function handlePythonMessage(data) {
    switch(data.type) {
        case 'completion':
            updateCompletion(data.content);
            break;
        case 'error':
            showError(data.content);
            break;
        case 'status':
            updateStatus(data.content);
            break;
        default:
            console.log('Unknown message type:', data.type);
    }
}

function sendToPython(data) {
    if (ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify(data));
    } else {
        console.error('WebSocket not connected');
    }
}

// Export functions for use in HTML
window.sendToPython = sendToPython;