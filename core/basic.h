#pragma once

#include <string>

namespace emw {

constexpr int kMaxRows = 32767;
constexpr int kMaxCols = 256;

struct Address {
    int row = -1;
    int col = -1;

    bool is_valid() const;
    std::string to_string() const;
    static bool TryParse(const std::string& s, Address* out);
};

enum class ValueType {
    Empty,
    Number,
    Text,
    Error
};

struct Value {
    ValueType type = ValueType::Empty;
    double number = 0.0;
    std::string text;

    static Value Empty();
    static Value Number(double v);
    static Value Text(std::string s);
    static Value Error(std::string code = "#NA");

    bool is_error() const;
    bool is_number() const;
    bool is_text() const;
    bool is_empty() const;

    std::string to_string() const;
};

struct Cell {
    std::string raw;
    Value value = Value::Empty();

    bool is_formula() const {
        return !raw.empty() && raw[0] == '=';
    }
};

Value ParseRawValue(const std::string& raw);

}
