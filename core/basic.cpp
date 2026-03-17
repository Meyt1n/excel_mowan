#include "basic.h"

#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <sstream>

namespace emw {

bool Address::is_valid() const {
    return row >= 0 && row < kMaxRows && col >= 0 && col < kMaxCols;
}

std::string Address::to_string() const {
    if (!is_valid()) return "";
    int c = col;
    std::string letters;
    do {
        int rem = c % 26;
        letters.insert(letters.begin(), char('A' + rem));
        c = c / 26 - 1;
    } while (c >= 0);
    return letters + std::to_string(row + 1);
}

static bool is_upper_alpha(char ch) {
    return ch >= 'A' && ch <= 'Z';
}

bool Address::TryParse(const std::string& s, Address* out) {
    if (!out) return false;

    int i = 0;
    int col_val = 0;
    int letters = 0;
    while (i < static_cast<int>(s.size()) && is_upper_alpha(s[i])) {
        col_val = col_val * 26 + (s[i] - 'A' + 1);
        i++;
        letters++;
    }
    if (letters == 0) return false;
    if (i < static_cast<int>(s.size()) && std::isalpha(static_cast<unsigned char>(s[i]))) return false;

    int row_val = 0;
    int digits = 0;
    while (i < static_cast<int>(s.size()) && std::isdigit(static_cast<unsigned char>(s[i]))) {
        row_val = row_val * 10 + (s[i] - '0');
        i++;
        digits++;
    }
    if (digits == 0 || i != static_cast<int>(s.size())) return false;

    Address addr{row_val - 1, col_val - 1};
    if (!addr.is_valid()) return false;
    *out = addr;
    return true;
}

Value Value::Empty() {
    return Value{};
}

Value Value::Number(double v) {
    Value out;
    out.type = ValueType::Number;
    out.number = v;
    return out;
}

Value Value::Text(std::string s) {
    Value out;
    out.type = ValueType::Text;
    out.text = std::move(s);
    return out;
}

Value Value::Error(std::string code) {
    Value out;
    out.type = ValueType::Error;
    out.text = std::move(code);
    return out;
}

bool Value::is_error() const { return type == ValueType::Error; }
bool Value::is_number() const { return type == ValueType::Number; }
bool Value::is_text() const { return type == ValueType::Text; }
bool Value::is_empty() const { return type == ValueType::Empty; }

std::string Value::to_string() const {
    switch (type) {
        case ValueType::Empty:
            return "";
        case ValueType::Number: {
            std::ostringstream oss;
            oss << std::setprecision(15) << number;
            return oss.str();
        }
        case ValueType::Text:
            return text;
        case ValueType::Error:
            return text.empty() ? "#NA" : text;
        default:
            return "#NA";
    }
}

Value ParseRawValue(const std::string& raw) {
    if (raw.empty()) return Value::Empty();

    size_t start = 0;
    size_t end = raw.size();
    while (start < end && std::isspace(static_cast<unsigned char>(raw[start]))) start++;
    while (end > start && std::isspace(static_cast<unsigned char>(raw[end - 1]))) end--;
    if (start >= end) return Value::Empty();

    std::string trimmed = raw.substr(start, end - start);
    char* endptr = nullptr;
    double number = std::strtod(trimmed.c_str(), &endptr);
    if (endptr != trimmed.c_str() && *endptr == '\0') {
        return Value::Number(number);
    }
    return Value::Text(trimmed);
}

}
