#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "basic.h"
#include "formula.h"

namespace emw {

class SpreadsheetGrid {
public:
    SpreadsheetGrid();

    bool SetCell(const Address& addr, const std::string& raw, std::string* error = nullptr);
    Value GetValue(const Address& addr);
    std::string GetRaw(const Address& addr) const;

    void Clear();
    void RecalcAll();
    void ForEachCell(const std::function<void(const Address&, const Cell&)>& fn) const;

private:
    struct CellState {
        Cell cell;
        std::unique_ptr<Node> ast;
        std::vector<Address> deps;
        bool dirty = true;
        bool formula_error = false;
    };

    int Key(const Address& addr) const;
    Value EvalCellInternal(const Address& addr, std::unordered_map<int, int>& state);
    std::unordered_map<int, std::vector<Address>> BuildDependencyMap() const;

    std::unordered_map<int, CellState> cells_;
};

using Grid = SpreadsheetGrid;

}
