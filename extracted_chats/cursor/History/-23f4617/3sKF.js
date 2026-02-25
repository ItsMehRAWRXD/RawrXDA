import { apiReadDir } from "../system/fs.js";

export async function mountSidebar() {
    const root = "D:/ProjectIDEAI";
    const files = await apiReadDir(root);
    const el = document.getElementById("sidebar");
    el.innerHTML = `<input id='fs-search' placeholder='Search...' /> <div id='tree'></div>`;
    const tree = document.getElementById("tree");
    tree.innerHTML = "";

    files.forEach(f => {
        const d = document.createElement("div");
        d.textContent = f.name;
        d.className = f.isDir ? "folder" : "file";
        d.onclick = () => {
            document.dispatchEvent(new CustomEvent("openFile", { detail: f }));
        };
        tree.appendChild(d);
    });
}

