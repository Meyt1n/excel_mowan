#include "formula.h"

#include <cmath>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <unordered_set>

using namespace std;


namespace emw {

unique_ptr<Node> Node::MakeNumber(double v) {
    auto n = make_unique<Node>();
    n->kind = Kind::Number;
    n->number = v;
    return n;
}

unique_ptr<Node> Node::MakeString(string s) {
    auto n = make_unique<Node>();
    n->kind = Kind::String;
    n->text = move(s);
    return n;
}

unique_ptr<Node> Node::MakeCell(const Address& a) {
    auto n = make_unique<Node>();
    n->kind = Kind::Cell;
    n->cell = a;
    return n;
}

unique_ptr<Node> Node::MakeRange(const Address& a, const Address& b) {
    auto n = make_unique<Node>();
    n->kind = Kind::Range;
    n->range = Range{a, b};
    return n;
}

unique_ptr<Node> Node::MakeUnary(char op, unique_ptr<Node> expr) {
    auto n = make_unique<Node>();
    n->kind = Kind::Unary;
    n->op = op;
    n->left = move(expr);
    return n;
}

unique_ptr<Node> Node::MakeBinary(char op, unique_ptr<Node> lhs, unique_ptr<Node> rhs) {
    auto n = make_unique<Node>();
    n->kind = Kind::Binary;
    n->op = op;
    n->left = move(lhs);
    n->right = move(rhs);
    return n;
}

unique_ptr<Node> Node::MakeFunc(string name, vector<unique_ptr<Node>> args) {
    auto n = make_unique<Node>();
    n->kind = Kind::Func;
    n->text = move(name);
    n->args = move(args);
    return n;
}

Parser::Lexer::Lexer(string input) : input_(move(input)) {}

Parser::Token Parser::Lexer::Peek() {
    if (!has_peek_) {
        peek_ = ReadToken();
        has_peek_ = true;
    }
    return peek_;
}

Parser::Token Parser::Lexer::Next() {
    if (has_peek_) {
        has_peek_ = false;
        return peek_;
    }
    return ReadToken();
}

void Parser::Lexer::SkipWhitespace() {
    while (pos_ < input_.size() && isspace(static_cast<unsigned char>(input_[pos_]))) pos_++;
}

static bool is_alpha(char ch) {
    return isalpha(static_cast<unsigned char>(ch)) != 0;
}

static bool is_alnum(char ch) {
    return isalnum(static_cast<unsigned char>(ch)) != 0;
}

Parser::Token Parser::Lexer::ReadToken() {
    SkipWhitespace();
    if (pos_ >= input_.size()) {
        return Token{TokenType::End, ""};
    }

    char ch = input_[pos_];
    switch (ch) {
        case '(': pos_++; return Token{TokenType::LParen, "("};
        case ')': pos_++; return Token{TokenType::RParen, ")"};
        case '+': pos_++; return Token{TokenType::Plus, "+"};
        case '-': pos_++; return Token{TokenType::Minus, "-"};
        case '*': pos_++; return Token{TokenType::Star, "*"};
        case '/': pos_++; return Token{TokenType::Slash, "/"};
        case '%': pos_++; return Token{TokenType::Percent, "%"};
        case ':': pos_++; return Token{TokenType::Colon, ":"};
        case ',': pos_++; return Token{TokenType::Comma, ","};
        case '"': {
            pos_++;
            string out;
            while (pos_ < input_.size()) {
                char c = input_[pos_++];
                if (c == '"') {
                    if (pos_ < input_.size() && input_[pos_] == '"') {
                        out.push_back('"');
                        pos_++;
                        continue;
                    }
                    return Token{TokenType::String, out};
                }
                out.push_back(c);
            }
            return Token{TokenType::Invalid, "unterminated string"};
        }
        default:
            break;
    }

    if (isdigit(static_cast<unsigned char>(ch)) || ch == '.') {
        size_t start = pos_;
        bool saw_digit = false;
        while (pos_ < input_.size() && isdigit(static_cast<unsigned char>(input_[pos_]))) {
            pos_++;
            saw_digit = true;
        }
        if (pos_ < input_.size() && input_[pos_] == '.') {
            pos_++;
            while (pos_ < input_.size() && isdigit(static_cast<unsigned char>(input_[pos_]))) {
                pos_++;
                saw_digit = true;
            }
        }
        if (!saw_digit) {
            return Token{TokenType::Invalid, "invalid number"};
        }
        string text = input_.substr(start, pos_ - start);
        char* endptr = nullptr;
        double val = strtod(text.c_str(), &endptr);
        if (endptr == text.c_str()) {
            return Token{TokenType::Invalid, "invalid number"};
        }
        Token tok{TokenType::Number, text};
        tok.number = val;
        return tok;
    }

    if (is_alpha(ch)) {
        size_t start = pos_;
        pos_++;
        while (pos_ < input_.size() && is_alnum(input_[pos_])) pos_++;
        string text = input_.substr(start, pos_ - start);
        for (char& c : text) {
            if (c >= 'a' && c <= 'z') c = char(c - 'a' + 'A');
        }
        return Token{TokenType::Identifier, text};
    }

    pos_++;
    return Token{TokenType::Invalid, string("invalid char: ") + ch};
}

Parser::Parser(const string& input) : lexer_(input) {
    cur_ = lexer_.Next();
}

unique_ptr<Node> Parser::Parse() {
    auto expr = ParseExpression();
    if (!expr) return nullptr;
    if (cur_.type != TokenType::End && error_.empty()) {
        error_ = "unexpected token at end";
        return nullptr;
    }
    return expr;
}

unique_ptr<Node> Parser::ParseExpression() {
    auto node = ParseTerm();
    if (!node) return nullptr;
    while (cur_.type == TokenType::Plus || cur_.type == TokenType::Minus) {
        char op = (cur_.type == TokenType::Plus) ? '+' : '-';
        Accept(cur_.type);
        auto rhs = ParseTerm();
        if (!rhs) return nullptr;
        node = Node::MakeBinary(op, move(node), move(rhs));
    }
    return node;
}

unique_ptr<Node> Parser::ParseTerm() {
    auto node = ParseUnary();
    if (!node) return nullptr;
    while (cur_.type == TokenType::Star || cur_.type == TokenType::Slash || cur_.type == TokenType::Percent) {
        char op = '*';
        if (cur_.type == TokenType::Slash) op = '/';
        else if (cur_.type == TokenType::Percent) op = '%';
        Accept(cur_.type);
        auto rhs = ParseUnary();
        if (!rhs) return nullptr;
        node = Node::MakeBinary(op, move(node), move(rhs));
    }
    return node;
}

unique_ptr<Node> Parser::ParseUnary() {
    if (cur_.type == TokenType::Plus || cur_.type == TokenType::Minus) {
        char op = (cur_.type == TokenType::Plus) ? '+' : '-';
        Accept(cur_.type);
        auto expr = ParseUnary();
        if (!expr) return nullptr;
        return Node::MakeUnary(op, move(expr));
    }
    return ParsePrimary();
}

unique_ptr<Node> Parser::ParsePrimary() {
    if (cur_.type == TokenType::Number) {
        double v = cur_.number;
        Accept(TokenType::Number);
        return Node::MakeNumber(v);
    }
    if (cur_.type == TokenType::String) {
        string s = cur_.text;
        Accept(TokenType::String);
        return Node::MakeString(move(s));
    }
    if (cur_.type == TokenType::Identifier) {
        string name = cur_.text;
        Accept(TokenType::Identifier);
        if (cur_.type == TokenType::LParen) {
            if (!IsFunctionName(name)) {
                error_ = "invalid function name";
                return nullptr;
            }
            Accept(TokenType::LParen);
            vector<unique_ptr<Node>> args;
            if (cur_.type != TokenType::RParen) {
                while (true) {
                    auto arg = ParseExpression();
                    if (!arg) return nullptr;
                    args.push_back(move(arg));
                    if (cur_.type == TokenType::Comma) {
                        Accept(TokenType::Comma);
                        continue;
                    }
                    break;
                }
            }
            if (!Expect(TokenType::RParen, "expected ')'")) return nullptr;
            return Node::MakeFunc(move(name), move(args));
        }
        return ParseCellOrRange(name);
    }
    if (cur_.type == TokenType::LParen) {
        Accept(TokenType::LParen);
        auto expr = ParseExpression();
        if (!expr) return nullptr;
        if (!Expect(TokenType::RParen, "expected ')'")) return nullptr;
        return expr;
    }

    error_ = "unexpected token";
    return nullptr;
}

unique_ptr<Node> Parser::ParseCellOrRange(const string& name) {
    Address addr;
    if (!Address::TryParse(name, &addr)) {
        error_ = "invalid cell reference";
        return nullptr;
    }

    if (cur_.type == TokenType::Colon) {
        Accept(TokenType::Colon);
        if (cur_.type != TokenType::Identifier) {
            error_ = "range requires cell reference";
            return nullptr;
        }
        string name2 = cur_.text;
        Accept(TokenType::Identifier);
        Address addr2;
        if (!Address::TryParse(name2, &addr2)) {
            error_ = "invalid range cell";
            return nullptr;
        }
        return Node::MakeRange(addr, addr2);
    }

    return Node::MakeCell(addr);
}

bool Parser::Accept(TokenType type) {
    if (cur_.type != type) return false;
    cur_ = lexer_.Next();
    if (cur_.type == TokenType::Invalid && error_.empty()) {
        error_ = cur_.text.empty() ? "invalid token" : cur_.text;
    }
    return true;
}

bool Parser::Expect(TokenType type, const char* msg) {
    if (Accept(type)) return true;
    if (error_.empty()) error_ = msg;
    return false;
}

bool Parser::IsFunctionName(const string& name) const {
    static const unordered_set<string> kFuncs = {
        "SIN", "COS", "SQRT", "ABS", "SUM", "AVG", "MIN", "MAX", "COUNT", "ROUND", "POW"
    };
    return kFuncs.find(name) != kFuncs.end();
}

Evaluator::Evaluator(EvalContext ctx) : ctx_(move(ctx)) {}

static Value EvalRangeFunction(
    const vector<unique_ptr<Node>>& args,
    const function<Value(const Range&)>& fn
) {
    if (args.size() != 1 || !fn) return Value::Error();
    const Node* arg = args[0].get();
    if (arg->kind != Node::Kind::Range) return Value::Error();
    return fn(arg->range);
}

Value Evaluator::Eval(const Node* node) {
    if (!node) return Value::Error();
    switch (node->kind) {
        case Node::Kind::Number:
            return Value::Number(node->number);
        case Node::Kind::String:
            return Value::Text(node->text);
        case Node::Kind::Cell:
            if (!ctx_.get_cell) return Value::Error();
            return ctx_.get_cell(node->cell);
        case Node::Kind::Range:
            return Value::Error();
        case Node::Kind::Unary:
            return EvalUnary(node->op, node->left.get());
        case Node::Kind::Binary:
            return EvalBinary(node->op, node->left.get(), node->right.get());
        case Node::Kind::Func:
            return EvalFunc(node->text, node->args);
        default:
            return Value::Error();
    }
}

Value Evaluator::EvalUnary(char op, const Node* expr) {
    Value v = Eval(expr);
    if (v.is_error()) return v;
    Value num = EnsureNumber(v);
    if (num.is_error()) return num;
    double n = num.number;
    if (op == '-') return Value::Number(-n);
    return Value::Number(n);
}

Value Evaluator::EvalBinary(char op, const Node* lhs, const Node* rhs) {
    Value a = Eval(lhs);
    if (a.is_error()) return a;
    Value b = Eval(rhs);
    if (b.is_error()) return b;

    if (op == '+') {
        if (a.is_text() || b.is_text()) {
            string sa = a.is_text() ? a.text : (a.is_empty() ? "" : NumberToString(a.number));
            string sb = b.is_text() ? b.text : (b.is_empty() ? "" : NumberToString(b.number));
            return Value::Text(sa + sb);
        }
        Value na = EnsureNumber(a);
        Value nb = EnsureNumber(b);
        if (na.is_error() || nb.is_error()) return Value::Error();
        return Value::Number(na.number + nb.number);
    }

    Value na = EnsureNumber(a);
    Value nb = EnsureNumber(b);
    if (na.is_error() || nb.is_error()) return Value::Error();

    switch (op) {
        case '-': return Value::Number(na.number - nb.number);
        case '*': return Value::Number(na.number * nb.number);
        case '/':
            if (nb.number == 0.0) return Value::Error();
            return Value::Number(na.number / nb.number);
        case '%':
            if (nb.number == 0.0) return Value::Error();
            return Value::Number(fmod(na.number, nb.number));
        default:
            return Value::Error();
    }
}

Value Evaluator::EvalFunc(const string& name, const vector<unique_ptr<Node>>& args) {
    if (name == "SIN" || name == "COS" || name == "SQRT" || name == "ABS") {
        if (args.size() != 1) return Value::Error();
        Value v = Eval(args[0].get());
        Value num = EnsureNumber(v);
        if (num.is_error()) return Value::Error();
        if (name == "SIN") {
            return Value::Number(sin(num.number));
        }
        if (name == "COS") {
            return Value::Number(cos(num.number));
        }
        if (name == "SQRT") {
            if (num.number < 0.0) return Value::Error();
            return Value::Number(sqrt(num.number));
        }
        return Value::Number(fabs(num.number));
    }

    if (name == "SUM") return EvalRangeFunction(args, ctx_.eval_range_sum);
    if (name == "AVG") return EvalRangeFunction(args, ctx_.eval_range_avg);
    if (name == "MIN") return EvalRangeFunction(args, ctx_.eval_range_min);
    if (name == "MAX") return EvalRangeFunction(args, ctx_.eval_range_max);
    if (name == "COUNT") return EvalRangeFunction(args, ctx_.eval_range_count);

    if (name == "ROUND") {
        if (args.size() != 1 && args.size() != 2) return Value::Error();
        Value value = EnsureNumber(Eval(args[0].get()));
        if (value.is_error()) return Value::Error();

        int digits = 0;
        if (args.size() == 2) {
            Value raw_digits = EnsureNumber(Eval(args[1].get()));
            if (raw_digits.is_error()) return Value::Error();
            digits = static_cast<int>(raw_digits.number);
        }

        double scale = pow(10.0, static_cast<double>(digits));
        if (!isfinite(scale) || scale == 0.0) return Value::Error();
        return Value::Number(round(value.number * scale) / scale);
    }

    if (name == "POW") {
        if (args.size() != 2) return Value::Error();
        Value base = EnsureNumber(Eval(args[0].get()));
        Value exponent = EnsureNumber(Eval(args[1].get()));
        if (base.is_error() || exponent.is_error()) return Value::Error();
        double out = pow(base.number, exponent.number);
        if (!isfinite(out)) return Value::Error();
        return Value::Number(out);
    }

    return Value::Error();
}

Value Evaluator::EnsureNumber(const Value& v) const {
    if (v.is_number()) return v;
    if (v.is_empty()) return Value::Number(0.0);
    return Value::Error();
}

string Evaluator::NumberToString(double v) const {
    ostringstream oss;
    oss << setprecision(15) << v;
    return oss.str();
}

string NormalizeFormulaInput(const string& raw) {
    if (raw.empty() || raw[0] != '=') return raw;

    string normalized = raw;
    bool in_string = false;
    for (size_t i = 1; i < normalized.size(); i++) {
        char c = normalized[i];
        if (c == '"') {
            if (in_string && i + 1 < normalized.size() && normalized[i + 1] == '"') {
                i++;
                continue;
            }
            in_string = !in_string;
            continue;
        }
        if (!in_string && c >= 'a' && c <= 'z') {
            normalized[i] = char(c - 'a' + 'A');
        }
    }
    return normalized;
}

}
