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
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.fetchCursorRuleContent = exports.fetchCursorRulesList = void 0;
const axios_1 = __importDefault(require("axios"));
const fs = __importStar(require("fs"));
const REPO_API_URL = 'https://api.github.com/repos/PatrickJS/awesome-cursorrules/contents/rules';
function fetchCursorRulesList() {
    return __awaiter(this, void 0, void 0, function* () {
        const response = yield axios_1.default.get(REPO_API_URL);
        return response.data.map((file) => ({
            name: file.name,
            download_url: file.download_url
        }));
    });
}
exports.fetchCursorRulesList = fetchCursorRulesList;
function fetchCursorRuleContent(ruleName, filePath, onProgress) {
    return __awaiter(this, void 0, void 0, function* () {
        const url = `${REPO_API_URL}/${ruleName}/.cursorrules`;
        const initialResponse = yield axios_1.default.get(url);
        const downloadUrl = initialResponse.data.download_url;
        const response = yield axios_1.default.get(downloadUrl, { responseType: 'stream' });
        const totalLength = parseInt(response.headers['content-length'], 10);
        let downloaded = 0;
        const writer = fs.createWriteStream(filePath);
        response.data.on('data', (chunk) => {
            downloaded += chunk.length;
            if (totalLength) {
                const progress = (downloaded / totalLength) * 100;
                onProgress(Math.round(progress));
            }
        });
        response.data.pipe(writer);
        return new Promise((resolve, reject) => {
            writer.on('finish', resolve);
            writer.on('error', reject);
        });
    });
}
exports.fetchCursorRuleContent = fetchCursorRuleContent;
//# sourceMappingURL=githubApi.js.map