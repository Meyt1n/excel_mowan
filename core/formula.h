#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "basic.h"

namespace emw {

struct Range {
    Address start;
    Address end;
};

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
    std::string text;
    Address cell;
    Range range;
    char op = 0;
    std::unique_ptr<Node> left;
    std::unique_ptr<Node> right;
    std::vector<std::unique_ptr<Node>> args;

    static std::unique_ptr<Node> MakeNumber(double v);
    static std::unique_ptr<Node> MakeString(std::string s);
    static std::unique_ptr<Node> MakeCell(const Address& a);
    static std::unique_ptr<Node> MakeRange(const Address& a, const Address& b);
    static std::unique_ptr<Node> MakeUnary(char op, std::unique_ptr<Node> expr);
    static std::unique_ptr<Node> MakeBinary(char op, std::unique_ptr<Node> lhs, std::unique_ptr<Node> rhs);
    static std::unique_ptr<Node> MakeFunc(std::string name, std::vector<std::unique_ptr<Node>> args);
};

class Parser {
public:
    explicit Parser(const std::string& input);
    std::unique_ptr<Node> Parse();
    const std::string& error() const { return error_; }

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
        std::string text;
        double number = 0.0;
    };

    class Lexer {
    public:
        explicit Lexer(std::string input);
        Token Peek();
        Token Next();

    private:
        Token ReadToken();
        void SkipWhitespace();

        std::string input_;
        size_t pos_ = 0;
        bool has_peek_ = false;
        Token peek_;
    };

    std::unique_ptr<Node> ParseExpression();
    std::unique_ptr<Node> ParseTerm();
    std::unique_ptr<Node> ParseUnary();
    std::unique_ptr<Node> ParsePrimary();
    std::unique_ptr<Node> ParseCellOrRange(const std::string& name);

    bool Accept(TokenType type);
    bool Expect(TokenType type, const char* msg);
    bool IsFunctionName(const std::string& name) const;

    Lexer lexer_;
    Token cur_;
    std::string error_;
};

struct EvalContext {
    std::function<Value(const Address&)> get_cell;
    std::function<Value(const Range&)> eval_range_sum;
    std::function<Value(const Range&)> eval_range_avg;
    std::function<Value(const Range&)> eval_range_min;
    std::function<Value(const Range&)> eval_range_max;
    std::function<Value(const Range&)> eval_range_count;
};

class Evaluator {
public:
    explicit Evaluator(EvalContext ctx);
    Value Eval(const Node* node);

private:
    Value EvalUnary(char op, const Node* expr);
    Value EvalBinary(char op, const Node* lhs, const Node* rhs);
    Value EvalFunc(const std::string& name, const std::vector<std::unique_ptr<Node>>& args);
    Value EnsureNumber(const Value& v) const;
    std::string NumberToString(double v) const;

    EvalContext ctx_;
};

std::string NormalizeFormulaInput(const std::string& raw);

}
