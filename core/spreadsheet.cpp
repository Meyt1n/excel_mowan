#include "spreadsheet.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <unordered_set>

namespace emw {

static int DependencyKey(const Address& addr) {
    return addr.row * kMaxCols + addr.col;
}

static std::vector<Address> CollectDependencies(const Node* node) {
    std::vector<Address> out;
    std::unordered_set<int> seen;

    std::function<void(const Node*)> walk = [&](const Node* n) {
        if (!n) return;
        switch (n->kind) {
            case Node::Kind::Cell: {
                int key = DependencyKey(n->cell);
                if (seen.insert(key).second) out.push_back(n->cell);
                break;
            }
            case Node::Kind::Range: {
                int r1 = std::min(n->range.start.row, n->range.end.row);
                int r2 = std::max(n->range.start.row, n->range.end.row);
                int c1 = std::min(n->range.start.col, n->range.end.col);
                int c2 = std::max(n->range.start.col, n->range.end.col);
                for (int r = r1; r <= r2; r++) {
                    for (int c = c1; c <= c2; c++) {
                        Address a{r, c};
                        int key = DependencyKey(a);
                        if (seen.insert(key).second) out.push_back(a);
                    }
                }
                break;
            }
            case Node::Kind::Unary:
                walk(n->left.get());
                break;
            case Node::Kind::Binary:
                walk(n->left.get());
                walk(n->right.get());
                break;
            case Node::Kind::Func:
                for (const auto& arg : n->args) walk(arg.get());
                break;
            default:
                break;
        }
    };

    walk(node);
    return out;
}

static bool WouldCreateCycle(
    int key,
    const std::vector<Address>& new_deps,
    const std::unordered_map<int, std::vector<Address>>& existing_deps
) {
    std::unordered_map<int, int> state;

    auto get_deps = [&](int current) {
        std::vector<int> deps;
        if (current == key) {
            deps.reserve(new_deps.size());
            for (const auto& addr : new_deps) deps.push_back(DependencyKey(addr));
            return deps;
        }
        auto it = existing_deps.find(current);
        if (it == existing_deps.end()) return deps;
        deps.reserve(it->second.size());
        for (const auto& addr : it->second) deps.push_back(DependencyKey(addr));
        return deps;
    };

    std::function<bool(int)> dfs = [&](int v) {
        auto it = state.find(v);
        if (it != state.end()) {
            if (it->second == 1) return true;
            if (it->second == 2) return false;
        }
        state[v] = 1;
        for (int to : get_deps(v)) {
            if (dfs(to)) return true;
        }
        state[v] = 2;
        return false;
    };

    return dfs(key);
}

static void NormalizeRangeBounds(const Range& range, Address* start, Address* end) {
    *start = Address{
        std::min(range.start.row, range.end.row),
        std::min(range.start.col, range.end.col)
    };
    *end = Address{
        std::max(range.start.row, range.end.row),
        std::max(range.start.col, range.end.col)
    };
}

static Value CalcRangeSum(const Range& range, const std::function<Value(const Address&)>& get_cell) {
    Address start;
    Address end;
    NormalizeRangeBounds(range, &start, &end);

    double sum = 0.0;
    for (int row = start.row; row <= end.row; row++) {
        for (int col = start.col; col <= end.col; col++) {
            Value v = get_cell(Address{row, col});
            if (v.is_error() || v.is_text()) return Value::Error();
            if (v.is_number()) sum += v.number;
        }
    }
    return Value::Number(sum);
}

static Value CalcRangeAvg(const Range& range, const std::function<Value(const Address&)>& get_cell) {
    Address start;
    Address end;
    NormalizeRangeBounds(range, &start, &end);

    double sum = 0.0;
    long long count = 0;
    for (int row = start.row; row <= end.row; row++) {
        for (int col = start.col; col <= end.col; col++) {
            Value v = get_cell(Address{row, col});
            if (v.is_error() || v.is_text()) return Value::Error();
            if (v.is_number()) {
                sum += v.number;
                count++;
            }
        }
    }
    if (count == 0) return Value::Error();
    return Value::Number(sum / static_cast<double>(count));
}

static Value CalcRangeMin(const Range& range, const std::function<Value(const Address&)>& get_cell) {
    Address start;
    Address end;
    NormalizeRangeBounds(range, &start, &end);

    double min_value = std::numeric_limits<double>::infinity();
    bool has_number = false;
    for (int row = start.row; row <= end.row; row++) {
        for (int col = start.col; col <= end.col; col++) {
            Value v = get_cell(Address{row, col});
            if (v.is_error() || v.is_text()) return Value::Error();
            if (v.is_number()) {
                min_value = std::min(min_value, v.number);
                has_number = true;
            }
        }
    }
    if (!has_number) return Value::Error();
    return Value::Number(min_value);
}

static Value CalcRangeMax(const Range& range, const std::function<Value(const Address&)>& get_cell) {
    Address start;
    Address end;
    NormalizeRangeBounds(range, &start, &end);

    double max_value = -std::numeric_limits<double>::infinity();
    bool has_number = false;
    for (int row = start.row; row <= end.row; row++) {
        for (int col = start.col; col <= end.col; col++) {
            Value v = get_cell(Address{row, col});
            if (v.is_error() || v.is_text()) return Value::Error();
            if (v.is_number()) {
                max_value = std::max(max_value, v.number);
                has_number = true;
            }
        }
    }
    if (!has_number) return Value::Error();
    return Value::Number(max_value);
}

static Value CalcRangeCount(const Range& range, const std::function<Value(const Address&)>& get_cell) {
    Address start;
    Address end;
    NormalizeRangeBounds(range, &start, &end);

    long long count = 0;
    for (int row = start.row; row <= end.row; row++) {
        for (int col = start.col; col <= end.col; col++) {
            Value v = get_cell(Address{row, col});
            if (v.is_error() || v.is_text()) return Value::Error();
            if (v.is_number()) count++;
        }
    }
    return Value::Number(static_cast<double>(count));
}

SpreadsheetGrid::SpreadsheetGrid() = default;

int SpreadsheetGrid::Key(const Address& addr) const {
    return addr.row * kMaxCols + addr.col;
}

bool SpreadsheetGrid::SetCell(const Address& addr, const std::string& raw, std::string* error) {
    if (!addr.is_valid()) {
        if (error) *error = "invalid address";
        return false;
    }

    std::string normalized_raw = NormalizeFormulaInput(raw);
    int key = Key(addr);
    if (normalized_raw.empty()) {
        cells_.erase(key);
        return true;
    }

    CellState& cell = cells_[key];
    cell.cell.raw = normalized_raw;
    cell.ast.reset();
    cell.deps.clear();
    cell.formula_error = false;
    cell.dirty = true;

    if (cell.cell.is_formula()) {
        Parser parser(cell.cell.raw.substr(1));
        auto ast = parser.Parse();
        if (!ast) {
            cell.formula_error = true;
            cell.cell.value = Value::Error();
            cell.dirty = false;
            if (error) *error = parser.error().empty() ? "invalid formula" : parser.error();
            return false;
        }

        auto deps = CollectDependencies(ast.get());
        if (WouldCreateCycle(key, deps, BuildDependencyMap())) {
            cell.formula_error = true;
            cell.ast = std::move(ast);
            cell.deps = std::move(deps);
            cell.cell.value = Value::Error();
            cell.dirty = false;
            if (error) *error = "circular reference";
            return false;
        }

        cell.ast = std::move(ast);
        cell.deps = std::move(deps);
        return true;
    }

    cell.cell.value = ParseRawValue(normalized_raw);
    cell.dirty = false;
    return true;
}

Value SpreadsheetGrid::GetValue(const Address& addr) {
    std::unordered_map<int, int> state;
    return EvalCellInternal(addr, state);
}

std::string SpreadsheetGrid::GetRaw(const Address& addr) const {
    if (!addr.is_valid()) return "";
    int key = Key(addr);
    auto it = cells_.find(key);
    if (it == cells_.end()) return "";
    return it->second.cell.raw;
}

void SpreadsheetGrid::Clear() {
    cells_.clear();
}

void SpreadsheetGrid::RecalcAll() {
    for (auto& kv : cells_) {
        kv.second.dirty = true;
    }
    std::unordered_map<int, int> state;
    for (auto& kv : cells_) {
        Address addr{kv.first / kMaxCols, kv.first % kMaxCols};
        EvalCellInternal(addr, state);
    }
}

void SpreadsheetGrid::ForEachCell(const std::function<void(const Address&, const Cell&)>& fn) const {
    for (const auto& kv : cells_) {
        Address addr{kv.first / kMaxCols, kv.first % kMaxCols};
        fn(addr, kv.second.cell);
    }
}

Value SpreadsheetGrid::EvalCellInternal(const Address& addr, std::unordered_map<int, int>& state) {
    if (!addr.is_valid()) return Value::Error();

    int key = Key(addr);
    auto it = cells_.find(key);
    if (it == cells_.end()) {
        return Value::Empty();
    }

    CellState& cell = it->second;
    auto st_it = state.find(key);
    if (st_it != state.end()) {
        if (st_it->second == 1) return Value::Error();
        if (st_it->second == 2) return cell.cell.value;
    }

    if (!cell.dirty) return cell.cell.value;

    state[key] = 1;

    if (cell.cell.is_formula()) {
        if (cell.formula_error || !cell.ast) {
            cell.cell.value = Value::Error();
            cell.dirty = false;
            state[key] = 2;
            return cell.cell.value;
        }

        EvalContext ctx;
        ctx.get_cell = [this, &state](const Address& a) { return this->EvalCellInternal(a, state); };
        ctx.eval_range_sum = [this, &state](const Range& range) {
            return CalcRangeSum(range, [this, &state](const Address& a) { return this->EvalCellInternal(a, state); });
        };
        ctx.eval_range_avg = [this, &state](const Range& range) {
            return CalcRangeAvg(range, [this, &state](const Address& a) { return this->EvalCellInternal(a, state); });
        };
        ctx.eval_range_min = [this, &state](const Range& range) {
            return CalcRangeMin(range, [this, &state](const Address& a) { return this->EvalCellInternal(a, state); });
        };
        ctx.eval_range_max = [this, &state](const Range& range) {
            return CalcRangeMax(range, [this, &state](const Address& a) { return this->EvalCellInternal(a, state); });
        };
        ctx.eval_range_count = [this, &state](const Range& range) {
            return CalcRangeCount(range, [this, &state](const Address& a) { return this->EvalCellInternal(a, state); });
        };

        Evaluator evaluator(ctx);
        cell.cell.value = evaluator.Eval(cell.ast.get());
    } else {
        cell.cell.value = ParseRawValue(cell.cell.raw);
    }

    cell.dirty = false;
    state[key] = 2;
    return cell.cell.value;
}

std::unordered_map<int, std::vector<Address>> SpreadsheetGrid::BuildDependencyMap() const {
    std::unordered_map<int, std::vector<Address>> deps;
    for (const auto& kv : cells_) {
        deps.emplace(kv.first, kv.second.deps);
    }
    return deps;
}

}
