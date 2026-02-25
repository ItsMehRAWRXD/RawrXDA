import { apiReadDir, apiReadFile } from "../system/fs.js";

const rootPath = "D:/ProjectIDEAI";

export async function mountSidebar() {
    const el = document.getElementById("sidebar");
    el.innerHTML = `
        <div id='fs-header'>
            <input id='fs-search' placeholder='Search files…' />
        </div>
        <div id='tree'></div>
    `;
    const tree = document.getElementById("tree");
    const search = document.getElementById("fs-search");
    search.addEventListener("input", debounce(async e => {
        const term = e.target.value.trim();
        if (!term) {
            renderTree(rootPath, tree);
            return;
        }
        await searchFiles(term, tree);
    }, 300));
    renderTree(rootPath, tree);
}

async function renderTree(path, container) {
    container.innerHTML = "";
    try {
        const entries = await apiReadDir(path);
        const dirs = entries.filter(e => e.isDir);
        const files = entries.filter(e => !e.isDir);

        [...dirs, ...files].forEach(entry => {
            const node = document.createElement("div");
            node.className = `tree-item ${entry.isDir ? "folder" : "file"}`;
            node.textContent = entry.name;
            container.appendChild(node);

            if (entry.isDir) {
                const children = document.createElement("div");
                children.className = "tree-children";
                node.appendChild(children);
                node.addEventListener("click", async e => {
                    e.stopPropagation();
                    if (children.childElementCount === 0) {
                        children.innerHTML = "<div class='tree-loading'>Loading…</div>";
                        await renderTree(entry.path, children);
                    } else {
                        children.classList.toggle("collapsed");
                    }
                });
            } else {
                node.addEventListener("click", async e => {
                    e.stopPropagation();
                    await openFile(entry.path);
                });
            }
        });
    } catch (error) {
        container.innerHTML = `<div class='tree-error'>${error.message}</div>`;
    }
}

async function openFile(path) {
    try {
        const file = await apiReadFile(path);
        document.dispatchEvent(new CustomEvent("openFile", { detail: file }));
    } catch (error) {
        document.dispatchEvent(new CustomEvent("notify", { detail:{ type:"error", message:error.message }}));
    }
}

async function searchFiles(query, container) {
    container.innerHTML = "<div class='tree-loading'>Searching…</div>";
    try {
        // naive search: only current root directory listing
        const entries = await apiReadDir(rootPath);
        const matches = entries.filter(entry => entry.name.toLowerCase().includes(query.toLowerCase()));
        if (matches.length === 0) {
            container.innerHTML = "<div class='tree-loading'>No matches.</div>";
            return;
        }
        container.innerHTML = "";
        matches.forEach(entry => {
            const node = document.createElement("div");
            node.className = `tree-item ${entry.isDir ? "folder" : "file"}`;
            node.textContent = entry.name;
            node.addEventListener("click", async e => {
                e.stopPropagation();
                if (entry.isDir) {
                    await renderTree(entry.path, container);
                } else {
                    await openFile(entry.path);
                }
            });
            container.appendChild(node);
        });
    } catch (error) {
        container.innerHTML = `<div class='tree-error'>${error.message}</div>`;
    }
}

function debounce(fn, wait) {
    let timer;
    return (...args) => {
        clearTimeout(timer);
        timer = setTimeout(() => fn(...args), wait);
    };
}

