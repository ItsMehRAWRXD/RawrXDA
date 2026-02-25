"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.parseURLs = parseURLs;
exports.findUrlAtPosition = findUrlAtPosition;
function parseURLs(text) {
    const urlRegex = new RegExp(`(https?|ftp):\\/\\/` + // scheme
        `(\\.?[a-zA-Z0-9_\\-])+` + // subdomain
        `(\\.[a-zA-Z]+)?` + // domain
        `(:[0-9]+)?` + // port
        `(\\.?[a-zA-Z0-9%_=~/+\\-@]+)*` + // path
        `(\\?([.,]?[a-zA-Z0-9&%_=~+\\-@])*)?` + // query
        `(#[a-zA-Z0-9&%_=~+\\-@]*)?`, // fragment
    "g");
    return text.match(urlRegex) || [];
}
function findUrlAtPosition(text, position) {
    const urls = parseURLs(text);
    // Track current position while iterating through the text
    let currentPos = 0;
    for (const url of urls) {
        // Find the start position of this URL in the text
        const startPos = text.indexOf(url, currentPos);
        if (startPos === -1)
            continue;
        // Update the current position for the next iteration
        currentPos = startPos + url.length;
        // Check if the given position is within this URL
        if (position >= startPos && position <= startPos + url.length) {
            return url;
        }
    }
    return undefined;
}
//# sourceMappingURL=urlDetection.js.map