"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.CodeShareAPI = void 0;
const got_1 = require("got");
const Constants_1 = require("./Constants");
const vscode = require("vscode");
const fs = require("fs");
const moment = require("moment");
const UIUtils_1 = require("./UIUtils");
const extension_1 = require("./extension");
class CodeShareAPI {
    static async createShare(text, language, password, showError) {
        let jsonRequest = {
            share: text,
            language: language,
            source: "vscode"
        };
        if (password !== undefined) {
            jsonRequest["password"] = password;
        }
        let response;
        try {
            response = await got_1.default.post(`${Constants_1.Constants.apiURL}/share/create`, {
                json: jsonRequest,
                rejectUnauthorized: false
            });
        }
        catch (error) {
            response = error.response;
        }
        if (showError) {
            if (response.statusCode !== 200) {
                const jsonResponse = JSON.parse(response.body);
                const result = await vscode.window.showErrorMessage("Error while creating code share", "More Details");
                if (result?.toLowerCase() === "more details") {
                    const displayJson = {
                        statusCode: response.statusCode,
                        response: jsonResponse
                    };
                    UIUtils_1.UIUtils.showErrorView(JSON.stringify(displayJson));
                }
                return undefined;
            }
        }
        if (response.statusCode === 200) {
            const shareHistoryPath = extension_1.extensionFolder + "/shareHistory.json";
            // read share history
            const shareHistory = this.getShareHistory();
            // check if history has reached 100 -> remove first item
            if (shareHistory.length >= 100) {
                shareHistory.shift;
            }
            // get share id
            const jsonResponse = JSON.parse(response.body);
            const shareID = jsonResponse.data.id;
            const newShare = {
                shareID: shareID,
                date: moment().toDate(),
                type: "normal"
            };
            if (password !== undefined) {
                newShare["type"] = "password";
            }
            // save share
            shareHistory.push(newShare);
            // write share history
            fs.writeFileSync(shareHistoryPath, JSON.stringify(shareHistory));
            // refresh view
            extension_1.codeShareProvider.refresh();
        }
        return response;
    }
    static getShareURL(shareID) {
        return `${Constants_1.Constants.appURL}/${shareID}`;
    }
    static getShareHistory() {
        const shareHistoryPath = extension_1.extensionFolder + "/shareHistory.json";
        // check if file exists -> create file
        if (!fs.existsSync(shareHistoryPath)) {
            fs.writeFileSync(shareHistoryPath, JSON.stringify([]));
        }
        // read share history and return
        return JSON.parse(fs.readFileSync(shareHistoryPath).toString());
    }
    static removeShareHistoryItem(shareID) {
        const shareHistoryPath = extension_1.extensionFolder + "/shareHistory.json";
        const newShareHistory = new Array();
        // get share history
        const shareHistory = this.getShareHistory();
        for (let i = 0; i < shareHistory.length; i++) {
            const item = shareHistory[i];
            if (item.shareID === shareID) {
                continue;
            }
            newShareHistory.push(item);
        }
        // save data in file
        fs.writeFileSync(shareHistoryPath, JSON.stringify(newShareHistory));
    }
}
exports.CodeShareAPI = CodeShareAPI;
//# sourceMappingURL=CodeShareAPI.js.map