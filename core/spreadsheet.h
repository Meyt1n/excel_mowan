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

struct EvaluatedCell {
    Value value = Value::Empty();
    bool has_error = false;
};

class SpreadsheetGrid {
public:
    SpreadsheetGrid();

    bool SetCell(const Address& addr, const string& raw, string* error = nullptr);
    Value GetValue(const Address& addr);
    bool HasError(const Address& addr);
    EvaluatedCell GetEvaluatedCell(const Address& addr);
    string GetRaw(const Address& addr) const;
    vector<Address> CollectAffectedCells(const vector<Address>& roots) const;

    void Clear();
    void RecalcAll();
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

    int Key(const Address& addr) const;
    Value EvalCellInternal(const Address& addr, unordered_map<int, int>& state);
    void ReplaceDependencies(int key, const vector<Address>& old_deps, const vector<Address>& new_deps);
    void MarkDependentsDirty(int key);

    unordered_map<int, CellState> cells_;
    unordered_map<int, vector<int>> reverse_deps_;
};

using Grid = SpreadsheetGrid;

}
