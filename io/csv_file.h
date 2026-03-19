#pragma once

#include <string>
#include <vector>

using namespace std;


namespace emw {

// 项目内使用的轻量 CSV 行解析/拼接工具。
class CsvFile {
public:
    // 将一行 CSV 拆分为字段（支持引号与双引号转义）。
    static vector<string> SplitLine(const string& line);

    // 将字段列表拼接为一行 CSV（按需加引号）。
    static string JoinLine(const vector<string>& fields);
};

}
