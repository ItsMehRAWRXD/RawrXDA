"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || function (mod) {
    if (mod && mod.__esModule) return mod;
    var result = {};
    if (mod != null) for (var k in mod) if (k !== "default" && Object.prototype.hasOwnProperty.call(mod, k)) __createBinding(result, mod, k);
    __setModuleDefault(result, mod);
    return result;
};
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.isPositiveMatchInGlobs = exports.isSameUri = exports.isWindowsUri = exports.convertClientPathToDebugger = exports.convertDebuggerPathToClient = void 0;
const file_url_1 = __importDefault(require("file-url"));
const url = __importStar(require("url"));
const Path = __importStar(require("path"));
const minimatch_1 = require("minimatch");
/** converts a server-side Xdebug file URI to a local path for VS Code with respect to source root settings */
function convertDebuggerPathToClient(fileUri, pathMapping) {
    let localSourceRootUrl;
    let serverSourceRootUrl;
    if (pathMapping) {
        for (const mappedServerPath of Object.keys(pathMapping)) {
            let mappedServerPathUrl = pathOrUrlToUrl(mappedServerPath);
            // try exact match
            if (fileUri.length === mappedServerPathUrl.length && isSameUri(fileUri, mappedServerPathUrl)) {
                // bail early
                serverSourceRootUrl = mappedServerPathUrl;
                localSourceRootUrl = pathOrUrlToUrl(pathMapping[mappedServerPath]);
                break;
            }
            // make sure it ends with a slash
            if (!mappedServerPathUrl.endsWith('/')) {
                mappedServerPathUrl += '/';
            }
            if (isSameUri(fileUri.substring(0, mappedServerPathUrl.length), mappedServerPathUrl)) {
                // If a matching mapping has previously been found, only update
                // it if the current server path is longer than the previous one
                // (longest prefix matching)
                if (!serverSourceRootUrl || mappedServerPathUrl.length > serverSourceRootUrl.length) {
                    serverSourceRootUrl = mappedServerPathUrl;
                    localSourceRootUrl = pathOrUrlToUrl(pathMapping[mappedServerPath]);
                    if (!localSourceRootUrl.endsWith('/')) {
                        localSourceRootUrl += '/';
                    }
                }
            }
        }
    }
    let localPath;
    if (serverSourceRootUrl && localSourceRootUrl) {
        fileUri = localSourceRootUrl + fileUri.substring(serverSourceRootUrl.length);
    }
    if (fileUri.startsWith('file://')) {
        const u = new URL(fileUri);
        let pathname = u.pathname;
        if (isWindowsUri(fileUri)) {
            // From Node.js lib/internal/url.js pathToFileURL
            pathname = pathname.replace(/\//g, Path.win32.sep);
            pathname = decodeURIComponent(pathname);
            if (u.hostname !== '') {
                localPath = `\\\\${url.domainToUnicode(u.hostname)}${pathname}`;
            }
            else {
                localPath = pathname.slice(1);
            }
        }
        else {
            localPath = decodeURIComponent(pathname);
        }
    }
    else {
        // if it's not a file url it could be sshfs or something else
        localPath = fileUri;
    }
    return localPath;
}
exports.convertDebuggerPathToClient = convertDebuggerPathToClient;
/** converts a local path from VS Code to a server-side Xdebug file URI with respect to source root settings */
function convertClientPathToDebugger(localPath, pathMapping) {
    let localSourceRootUrl;
    let serverSourceRootUrl;
    // Parse or convert local path to URL
    const localFileUri = pathOrUrlToUrl(localPath);
    let serverFileUri;
    if (pathMapping) {
        for (const mappedServerPath of Object.keys(pathMapping)) {
            //let mappedLocalSource = pathMapping[mappedServerPath]
            let mappedLocalSourceUrl = pathOrUrlToUrl(pathMapping[mappedServerPath]);
            // try exact match
            if (localFileUri.length === mappedLocalSourceUrl.length && isSameUri(localFileUri, mappedLocalSourceUrl)) {
                // bail early
                localSourceRootUrl = mappedLocalSourceUrl;
                serverSourceRootUrl = pathOrUrlToUrl(mappedServerPath);
                break;
            }
            // make sure it ends with a slash
            if (!mappedLocalSourceUrl.endsWith('/')) {
                mappedLocalSourceUrl += '/';
            }
            if (isSameUri(localFileUri.substring(0, mappedLocalSourceUrl.length), mappedLocalSourceUrl)) {
                // If a matching mapping has previously been found, only update
                // it if the current local path is longer than the previous one
                // (longest prefix matching)
                if (!localSourceRootUrl || mappedLocalSourceUrl.length > localSourceRootUrl.length) {
                    localSourceRootUrl = mappedLocalSourceUrl;
                    serverSourceRootUrl = pathOrUrlToUrl(mappedServerPath);
                    if (!serverSourceRootUrl.endsWith('/')) {
                        serverSourceRootUrl += '/';
                    }
                }
            }
        }
    }
    if (serverSourceRootUrl && localSourceRootUrl) {
        serverFileUri = serverSourceRootUrl + localFileUri.substring(localSourceRootUrl.length);
    }
    else {
        serverFileUri = localFileUri;
    }
    return serverFileUri;
}
exports.convertClientPathToDebugger = convertClientPathToDebugger;
function isWindowsUri(path) {
    return /^file:\/\/\/[a-zA-Z]:\//.test(path) || /^file:\/\/[^/]/.test(path);
}
exports.isWindowsUri = isWindowsUri;
function isWindowsPath(path) {
    return /^[a-zA-Z]:\\/.test(path) || /^\\\\/.test(path) || /^[a-zA-Z]:$/.test(path) || /^[a-zA-Z]:\//.test(path);
}
function pathOrUrlToUrl(path) {
    // Do not try to parse windows drive letter paths
    if (!isWindowsPath(path)) {
        try {
            // try to parse, but do not modify
            new URL(path).toString();
            // super simple relative path resolver
            return simpleResolveUrl(path);
        }
        catch (ex) {
            // should be a path
        }
    }
    // Not a URL, do some windows path mangling before it is converted to URL
    if (path.startsWith('\\\\')) {
        // UNC
        path = Path.win32.resolve(path);
        const hostEndIndex = path.indexOf('\\', 2);
        const host = path.substring(2, hostEndIndex);
        const outURL = new URL('file://');
        outURL.hostname = url.domainToASCII(host);
        outURL.pathname = path.substring(hostEndIndex).replace(/\\/g, '/');
        return outURL.toString();
    }
    if (/^[a-zA-Z]:$/.test(path)) {
        // if local source root mapping is only drive letter, add backslash
        path += '\\';
    }
    // Do not change drive later to lower case anymore
    // if (/^[a-zA-Z]:/.test(path)) {
    //     // Xdebug always lowercases Windows drive letters in file URIs
    //     //path = path.replace(/^[A-Z]:/, match => match.toLowerCase())
    // }
    path = isWindowsPath(path) ? Path.win32.resolve(path) : Path.posix.resolve(path);
    return (0, file_url_1.default)(path, { resolve: false });
}
function isSameUri(clientUri, debuggerUri) {
    if (isWindowsUri(clientUri) || isWindowsUri(debuggerUri)) {
        // compare case-insensitive on Windows
        return debuggerUri.toLowerCase() === clientUri.toLowerCase();
    }
    else {
        return debuggerUri === clientUri;
    }
}
exports.isSameUri = isSameUri;
function isPositiveMatchInGlobs(path, globs) {
    const f = globs.find(glob => (0, minimatch_1.minimatch)(path, glob.charAt(0) == '!' ? glob.substring(1) : glob));
    return f !== undefined && f.charAt(0) !== '!';
}
exports.isPositiveMatchInGlobs = isPositiveMatchInGlobs;
function simpleResolveUrl(path) {
    if (path.indexOf('/../') != -1) {
        const pp = path.split('/');
        let i;
        while ((i = pp.findIndex(v => v == '..')) > 0) {
            pp.splice(i - 1, 2);
        }
        path = pp.join('/');
    }
    return path;
}
//# sourceMappingURL=paths.js.map