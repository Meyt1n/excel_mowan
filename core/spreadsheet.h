#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "basic.h"
#include "formula.h"

using namespace std;


namespace emw {

// GUI 一次读取同时拿到值和错误状态，避免重复计算。
struct EvaluatedCell {
    Value value = Value::Empty();
    bool has_error = false;
};

class SpreadsheetGrid {
public:
    SpreadsheetGrid();

    // 写入用户原始输入（普通值或公式）。解析失败或循环引用时返回 false。
    bool SetCell(const Address& addr, const string& raw, string* error = nullptr);

    // 获取计算后的值（惰性计算 + 缓存）。
    Value GetValue(const Address& addr);

    // 当前单元格是否处于公式/计算错误状态。
    bool HasError(const Address& addr);

    // 一次获取值和错误标记（减少重复求值）。
    EvaluatedCell GetEvaluatedCell(const Address& addr);

    // 获取用户原始输入文本。
    string GetRaw(const Address& addr) const;

    // 从根节点收集受影响单元格（根节点 + 下游依赖）。
    vector<Address> CollectAffectedCells(const vector<Address>& roots) const;

    // 清空单元格与依赖信息。
    void Clear();

    // 强制全量重算。
    void RecalcAll();

    // 遍历所有非空单元格（稀疏存储）。
    void ForEachCell(const function<void(const Address&, const Cell&)>& fn) const;

private:
    struct CellState {
        Cell cell;
        unique_ptr<Node> ast;
        vector<Address> deps;
        bool dirty = true;
        bool formula_error = false;
        bool eval_error = false;
    };

    // 稀疏映射键生成函数。
    int Key(const Address& addr) const;

    // 内部递归求值，借助 state 做循环检测。
    Value EvalCellInternal(const Address& addr, unordered_map<int, int>& state);

    // 用新依赖替换旧依赖，更新反向依赖表。
    void ReplaceDependencies(int key, const vector<Address>& old_deps, const vector<Address>& new_deps);

    // 将所有下游依赖标记为 dirty。
    void MarkDependentsDirty(int key);

    unordered_map<int, CellState> cells_;
    unordered_map<int, vector<int>> reverse_deps_;
};

using Grid = SpreadsheetGrid;

}
