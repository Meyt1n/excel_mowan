#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "basic.h"

using namespace std;


namespace emw {

struct Range {
    Address start;
    Address end;
};

// 公式表达式的 AST 节点。
struct Node {
    enum class Kind {
        Number,
        String,
        Cell,
        Range,
        Unary,
        Binary,
        Func
    };

    Kind kind = Kind::Number;
    double number = 0.0;
    string text;
    Address cell;
    Range range;
    char op = 0;
    unique_ptr<Node> left;
    unique_ptr<Node> right;
    vector<unique_ptr<Node>> args;

    // AST 节点工厂函数。
    static unique_ptr<Node> MakeNumber(double v);
    static unique_ptr<Node> MakeString(string s);
    static unique_ptr<Node> MakeCell(const Address& a);
    static unique_ptr<Node> MakeRange(const Address& a, const Address& b);
    static unique_ptr<Node> MakeUnary(char op, unique_ptr<Node> expr);
    static unique_ptr<Node> MakeBinary(char op, unique_ptr<Node> lhs, unique_ptr<Node> rhs);
    static unique_ptr<Node> MakeFunc(string name, vector<unique_ptr<Node>> args);
};

// 递归下降解析器：把公式文本解析为 AST。
class Parser {
public:
    explicit Parser(const string& input);
    unique_ptr<Node> Parse();
    const string& error() const { return error_; }

private:
    enum class TokenType {
        End,
        Number,
        String,
        Identifier,
        LParen,
        RParen,
        Plus,
        Minus,
        Star,
        Slash,
        Percent,
        Colon,
        Comma,
        Invalid
    };

    struct Token {
        TokenType type = TokenType::Invalid;
        string text;
        double number = 0.0;
    };

    class Lexer {
    public:
        explicit Lexer(string input);
        Token Peek();
        Token Next();

    private:
        Token ReadToken();
        void SkipWhitespace();

        string input_;
        size_t pos_ = 0;
        bool has_peek_ = false;
        Token peek_;
    };

    unique_ptr<Node> ParseExpression();
    unique_ptr<Node> ParseTerm();
    unique_ptr<Node> ParseUnary();
    unique_ptr<Node> ParsePrimary();
    unique_ptr<Node> ParseCellOrRange(const string& name);

    bool Accept(TokenType type);
    bool Expect(TokenType type, const char* msg);
    bool IsFunctionName(const string& name) const;

    Lexer lexer_;
    Token cur_;
    string error_;
};

// 由 SpreadsheetGrid 提供的求值回调接口。
struct EvalContext {
    function<Value(const Address&)> get_cell;
    function<Value(const Range&)> eval_range_sum;
    function<Value(const Range&)> eval_range_avg;
    function<Value(const Range&)> eval_range_min;
    function<Value(const Range&)> eval_range_max;
    function<Value(const Range&)> eval_range_count;
};

class Evaluator {
public:
    explicit Evaluator(EvalContext ctx);
    Value Eval(const Node* node);

private:
    Value EvalUnary(char op, const Node* expr);
    Value EvalBinary(char op, const Node* lhs, const Node* rhs);
    Value EvalFunc(const string& name, const vector<unique_ptr<Node>>& args);
    Value EnsureNumber(const Value& v) const;
    string NumberToString(double v) const;

    EvalContext ctx_;
};

// 规范化公式输入：将字符串字面量之外的标识符转成大写。
string NormalizeFormulaInput(const string& raw);

}
