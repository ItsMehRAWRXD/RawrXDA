export function initStatusBar() {
    const left = document.getElementById("status-left");
    const right = document.getElementById("status-right");

    updateTime(right);
    setInterval(() => updateTime(right), 1000);

    left.innerHTML = `
        <span>BigDaddyG Ultimate IDE</span>
        <span id="status-mode">Mode: Dev</span>
        <span id="status-branch">Branch: main</span>
    `;
}

function updateTime(container) {
    const now = new Date();
    container.innerHTML = `
        <span>${now.toLocaleTimeString()}</span>
        <span>${now.toLocaleDateString()}</span>
    `;
}

