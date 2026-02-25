"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.varJsonProperty = void 0;
const crypto_1 = require("crypto");
const recursionMagic = `---MAGIC---${(0, crypto_1.randomUUID)()}---MAGIC---`;
/**
 * Generate a JSON object and pretty print it
 */
async function varJsonProperty(property) {
    const obj = await _varJsonProperty(property);
    const json = JSON.stringify(obj, null, ' ');
    return json.replace(new RegExp(`"${recursionMagic}"`, 'g'), '...');
}
exports.varJsonProperty = varJsonProperty;
// eslint-disable-next-line @typescript-eslint/no-explicit-any
async function _varJsonProperty(property, depth = 0) {
    if (depth >= 20) {
        // prevent infinite recursion
        return recursionMagic;
    }
    let displayValue;
    if (property.hasChildren || property.type === 'array' || property.type === 'object') {
        let properties;
        if (property.hasChildren) {
            if (property.children.length === property.numberOfChildren) {
                properties = property.children;
            }
            else {
                // TODO: also take into account the number of children for pagination
                properties = await property.getChildren();
            }
        }
        else {
            properties = [];
        }
        const obj = await Promise.all(properties.map(async (property) => {
            return [property.name, await _varJsonProperty(property, depth + 1)];
        }));
        // TODO: handle only numeric, sequential arrays?
        return Object.fromEntries(obj);
    }
    else {
        // for null, uninitialized, resource, etc. show the type
        displayValue = property.value || property.type === 'string' ? property.value : property.type;
        if (property.type === 'string') {
            // escaping ?
            if (property.size > property.value.length) {
                // get value
                const p2 = await property.context.stackFrame.connection.sendPropertyValueNameCommand(property.fullName, property.context);
                displayValue = p2.value;
            }
            return displayValue;
        }
        else if (property.type === 'bool') {
            return Boolean(parseInt(displayValue, 10));
        }
        else if (property.type === 'int') {
            return parseInt(displayValue, 10);
        }
        else if (property.type === 'float' || property.type === 'double') {
            return parseFloat(displayValue);
        }
        else {
            return property.value;
        }
    }
}
//# sourceMappingURL=varJson.js.map