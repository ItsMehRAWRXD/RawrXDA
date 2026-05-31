'use strict';

globalThis.handleRequest = async function handleRequest(request, rawrxd) {
    switch (request.type) {
    case 'file.read':
        return { content: await rawrxd.readTextFile(request.payload.path) };
    case 'file.write':
        return rawrxd.writeTextFile(request.payload.path, request.payload.content || request.payload.data || '');
    case 'network.request':
        return rawrxd.httpRequest(request.payload.url);
    case 'process.spawn':
    case 'process.exec':
        return rawrxd.spawnProcess(request.payload.command || 'cmd.exe');
    case 'addon.load':
        return rawrxd.requireModule(request.payload.name || 'ffi-napi');
    default:
        return { ok: true, type: request.type };
    }
};