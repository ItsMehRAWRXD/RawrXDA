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
Object.defineProperty(exports, "__esModule", { value: true });
exports.getConfiguredEnvironment = void 0;
const fs = __importStar(require("fs"));
const dotenv = __importStar(require("dotenv"));
/**
 * Returns the user-configured portion of the environment variables.
 */
function getConfiguredEnvironment(args) {
    if (args.envFile) {
        try {
            return merge(readEnvFile(args.envFile), args.env || {});
        }
        catch (e) {
            throw new Error('Failed reading envFile');
        }
    }
    return args.env || {};
}
exports.getConfiguredEnvironment = getConfiguredEnvironment;
function readEnvFile(file) {
    if (!fs.existsSync(file)) {
        return {};
    }
    const buffer = stripBOM(fs.readFileSync(file, 'utf8'));
    const env = dotenv.parse(Buffer.from(buffer));
    return env;
}
function stripBOM(s) {
    if (s && s[0] === '\uFEFF') {
        s = s.substring(1);
    }
    return s;
}
function merge(...vars) {
    if (process.platform === 'win32') {
        return caseInsensitiveMerge(...vars);
    }
    return Object.assign({}, ...vars);
}
/**
 * Performs a case-insenstive merge of the list of objects.
 */
function caseInsensitiveMerge(...objs) {
    if (objs.length === 0) {
        return {};
    }
    const out = {};
    const caseMapping = Object.create(null); // prototype-free object
    for (const obj of objs) {
        if (!obj) {
            continue;
        }
        for (const key of Object.keys(obj)) {
            const normalized = key.toLowerCase();
            if (caseMapping[normalized]) {
                out[caseMapping[normalized]] = obj[key];
            }
            else {
                caseMapping[normalized] = key;
                out[key] = obj[key];
            }
        }
    }
    return out;
}
//# sourceMappingURL=envfile.js.map