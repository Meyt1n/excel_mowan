#pragma once

#include <string>

using namespace std;


namespace emw {

// 表格边界（本项目采用类 Excel 的紧凑范围）。
constexpr int kMaxRows = 32767;
constexpr int kMaxCols = 256;

// 0-based 单元格坐标。
struct Address {
    int row = -1;
    int col = -1;

    // 判断坐标是否位于 [0, kMaxRows) x [0, kMaxCols) 内。
    bool is_valid() const;

    // 转成 Excel 风格地址（例如 A1、B3、AA20）。
    string to_string() const;

    // 将 Excel 风格地址解析为 0-based 坐标。
    static bool TryParse(const string& s, Address* out);
};

enum class ValueType {
    Empty,
    Number,
    Text,
    Error
};

// 公式层与显示层共用的运行期值类型。
struct Value {
    ValueType type = ValueType::Empty;
    double number = 0.0;
    string text;

    // 工厂函数。
    static Value Empty();
    static Value Number(double v);
    static Value Text(string s);
    static Value Error(string code = "#NA");

    // 类型判断。
    bool is_error() const;
    bool is_number() const;
    bool is_text() const;
    bool is_empty() const;

    // 转成显示字符串。
    string to_string() const;
};

// 网格中的原始单元格状态。
struct Cell {
    string raw;
    Value value = Value::Empty();

    // 以 '=' 开头的输入视为公式。
    bool is_formula() const {
        return !raw.empty() && raw[0] == '=';
    }
};

// 将用户输入解析为数字/文本/空值。
Value ParseRawValue(const string& raw);

}
