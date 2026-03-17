#include "csv_file.h"

namespace emw {

std::vector<std::string> CsvFile::SplitLine(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    bool in_quotes = false;

    for (size_t i = 0; i < line.size(); i++) {
        char ch = line[i];
        if (in_quotes) {
            if (ch == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    cur.push_back('"');
                    i++;
                } else {
                    in_quotes = false;
                }
            } else {
                cur.push_back(ch);
            }
        } else {
            if (ch == ',') {
                out.push_back(cur);
                cur.clear();
            } else if (ch == '"') {
                in_quotes = true;
            } else {
                cur.push_back(ch);
            }
        }
    }
    out.push_back(cur);
    return out;
}

std::string CsvFile::JoinLine(const std::vector<std::string>& fields) {
    std::string out;
    for (size_t i = 0; i < fields.size(); i++) {
        if (i > 0) out.push_back(',');
        const std::string& f = fields[i];
        bool need_quotes = false;
        for (char ch : f) {
            if (ch == '"' || ch == ',' || ch == '\n' || ch == '\r') {
                need_quotes = true;
                break;
            }
        }
        if (!need_quotes) {
            out += f;
        } else {
            out.push_back('"');
            for (char ch : f) {
                if (ch == '"') out += "\"\"";
                else out.push_back(ch);
            }
            out.push_back('"');
        }
    }
    return out;
}

}
