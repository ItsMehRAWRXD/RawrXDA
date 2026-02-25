"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.shouldIgnoreException = void 0;
function shouldIgnoreException(name, patterns) {
    return patterns.some(pattern => name.match(convertPattern(pattern)));
}
exports.shouldIgnoreException = shouldIgnoreException;
function convertPattern(pattern) {
    const esc = pattern.replace(/[|\\{}()[\]^$+*?.]/g, '\\$&').replace(/-/g, '\\x2d');
    const proc = esc.replace(/\\\*\\\*/g, '.*').replace(/\\\*/g, '[^\\\\]*');
    return '^' + proc + '$';
}
//# sourceMappingURL=ignore.js.map