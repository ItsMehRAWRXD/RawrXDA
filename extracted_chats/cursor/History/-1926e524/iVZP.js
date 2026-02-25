import * as monaco from "https://cdn.jsdelivr.net/npm/monaco-editor@0.45.0/+esm";

let editor;

export function initEditor() {
    editor = monaco.editor.create(document.getElementById("editor"), {
        value: "// BigDaddyG IDE Ready",
        language: "javascript",
        theme: "vs-dark",
        fontSize: 16,
        minimap: { enabled:false }
    });

    document.addEventListener("openFile", e => {
        editor.setValue(e.detail.content || "");
    });
}

