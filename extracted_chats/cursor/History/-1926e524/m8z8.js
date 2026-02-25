import * as monaco from "https://cdn.jsdelivr.net/npm/monaco-editor@0.45.0/+esm";
import { apiWriteFile } from "../system/fs.js";

const documents = new Map();
let editor;
let activePath = null;

export function initEditor() {
    editor = monaco.editor.create(document.getElementById("editor"), {
        value: "// BigDaddyG IDE Ready",
        language: "javascript",
        theme: "vs-dark",
        fontSize: 15,
        automaticLayout: true,
        minimap: { enabled:true },
        tabSize: 2
    });

    document.addEventListener("openFile", e => {
        const { path, content } = e.detail;
        openDocument(path, content);
    });

    editor.onDidChangeModelContent(() => {
        if (!activePath) return;
        const doc = documents.get(activePath);
        if (doc) {
            doc.dirty = doc.model.getValue() !== doc.original;
            emitDirtyChange(activePath, doc.dirty);
        }
    });

    document.addEventListener("keydown", async e => {
        if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === "s") {
            e.preventDefault();
            await saveActiveDocument();
        }
    });
}

function openDocument(path, content) {
    let doc = documents.get(path);
    if (!doc) {
        const model = monaco.editor.createModel(content, detectLanguage(path));
        doc = { model, original:content, dirty:false };
        documents.set(path, doc);
        dispatchTabUpdate(path);
    } else {
        doc.model.setValue(content);
        doc.original = content;
        doc.dirty = false;
    }
    activePath = path;
    editor.setModel(doc.model);
    dispatchTabActivate(path);
}

export function getActivePath() {
    return activePath;
}

export async function saveActiveDocument() {
    if (!activePath) return;
    const doc = documents.get(activePath);
    if (!doc || !doc.dirty) return;
    const content = doc.model.getValue();
    try {
        await apiWriteFile(activePath, content);
        doc.original = content;
        doc.dirty = false;
        dispatchTabUpdate(activePath);
        document.dispatchEvent(new CustomEvent("notify", {
            detail:{ type:"success", message:`Saved ${activePath}` }
        }));
    } catch (error) {
        document.dispatchEvent(new CustomEvent("notify", {
            detail:{ type:"error", message:`Save failed: ${error.message}` }
        }));
    }
}

function detectLanguage(path) {
    const ext = path.split(".").pop()?.toLowerCase();
    switch (ext) {
        case "js": return "javascript";
        case "ts": return "typescript";
        case "json": return "json";
        case "html": return "html";
        case "css": return "css";
        case "md": return "markdown";
        case "ps1": return "powershell";
        default: return "plaintext";
    }
}

function dispatchTabUpdate(path) {
    const doc = documents.get(path);
    document.dispatchEvent(new CustomEvent("updateTab", {
        detail:{ path, dirty:doc?.dirty }
    }));
}

function emitDirtyChange(path, dirty) {
    document.dispatchEvent(new CustomEvent("updateTab", {
        detail:{ path, dirty }
    }));
}

function dispatchTabActivate(path) {
    document.dispatchEvent(new CustomEvent("activateTab", {
        detail:{ path }
    }));
}

