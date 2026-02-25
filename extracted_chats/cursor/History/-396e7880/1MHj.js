let current;

export function initNotifications() {
    document.addEventListener("notify", e => {
        showNotification(e.detail);
    });
}

function showNotification({ type = "info", message = "", action } = {}) {
    clear();
    current = document.createElement("div");
    current.className = `notification-bar ${type}`;
    current.innerHTML = `
        <span>${message}</span>
        ${action ? `<button id="notify-action">${action.label}</button>` : ""}
        <button id="notify-close">×</button>
    `;
    document.body.appendChild(current);

    if (action) {
        document.getElementById("notify-action").addEventListener("click", () => {
            action.handler?.();
            clear();
        });
    }

    document.getElementById("notify-close").addEventListener("click", clear);
    setTimeout(clear, 5000);
}

function clear() {
    if (current) {
        current.remove();
        current = null;
    }
}

