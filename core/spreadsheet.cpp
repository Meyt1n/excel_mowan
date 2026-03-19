#include "spreadsheet.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <unordered_set>

using namespace std;


namespace emw {

static int DependencyKey(const Address& addr) {
    return addr.row * kMaxCols + addr.col;
}

static vector<Address> CollectDependencies(const Node* node) {
    // 从 AST 中收集所有单元格/范围依赖。
    vector<Address> out;
    unordered_set<int> seen;

    function<void(const Node*)> walk = [&](const Node* n) {
        if (!n) return;
        switch (n->kind) {
            case Node::Kind::Cell: {
                int key = DependencyKey(n->cell);
                if (seen.insert(key).second) out.push_back(n->cell);
                break;
            }
            case Node::Kind::Range: {
                int r1 = min(n->range.start.row, n->range.end.row);
                int r2 = max(n->range.start.row, n->range.end.row);
                int c1 = min(n->range.start.col, n->range.end.col);
                int c2 = max(n->range.start.col, n->range.end.col);
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
    const vector<Address>& new_deps,
    const function<const vector<Address>*(int)>& get_deps
) {
    // 判断新增依赖是否会回到自身，避免循环引用。
    unordered_set<int> visited;

    function<bool(int)> reaches_key = [&](int current) {
        if (current == key) return true;
        if (!visited.insert(current).second) return false;

        const vector<Address>* deps = get_deps ? get_deps(current) : nullptr;
        if (!deps) return false;
        for (const auto& dep : *deps) {
            if (reaches_key(DependencyKey(dep))) return true;
        }
        return false;
    };

    for (const auto& dep : new_deps) {
        visited.clear();
        if (reaches_key(DependencyKey(dep))) return true;
    }
    return false;
}

static void NormalizeRangeBounds(const Range& range, Address* start, Address* end) {
    // 把范围规范成左上到右下。
    *start = Address{
        min(range.start.row, range.end.row),
        min(range.start.col, range.end.col)
    };
    *end = Address{
        max(range.start.row, range.end.row),
        max(range.start.col, range.end.col)
    };
}

static Value CalcRangeSum(const Range& range, const function<Value(const Address&)>& get_cell) {
    // SUM：范围内所有数字相加。
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

static Value CalcRangeAvg(const Range& range, const function<Value(const Address&)>& get_cell) {
    // AVG：范围内数字平均值（无数字则报错）。
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
    if (count == 0) return Value::Number(0.0);
    return Value::Number(sum / static_cast<double>(count));
}

static Value CalcRangeMin(const Range& range, const function<Value(const Address&)>& get_cell) {
    // MIN：范围内最小数字（无数字则报错）。
    Address start;
    Address end;
    NormalizeRangeBounds(range, &start, &end);

    double min_value = numeric_limits<double>::infinity();
    bool has_number = false;
    for (int row = start.row; row <= end.row; row++) {
        for (int col = start.col; col <= end.col; col++) {
            Value v = get_cell(Address{row, col});
            if (v.is_error() || v.is_text()) return Value::Error();
            if (v.is_number()) {
                min_value = min(min_value, v.number);
                has_number = true;
            }
        }
    }
    if (!has_number) return Value::Error();
    return Value::Number(min_value);
}

static Value CalcRangeMax(const Range& range, const function<Value(const Address&)>& get_cell) {
    // MAX：范围内最大数字（无数字则报错）。
    Address start;
    Address end;
    NormalizeRangeBounds(range, &start, &end);

    double max_value = -numeric_limits<double>::infinity();
    bool has_number = false;
    for (int row = start.row; row <= end.row; row++) {
        for (int col = start.col; col <= end.col; col++) {
            Value v = get_cell(Address{row, col});
            if (v.is_error() || v.is_text()) return Value::Error();
            if (v.is_number()) {
                max_value = max(max_value, v.number);
                has_number = true;
            }
        }
    }
    if (!has_number) return Value::Error();
    return Value::Number(max_value);
}

static Value CalcRangeCount(const Range& range, const function<Value(const Address&)>& get_cell) {
    // COUNT：范围内数字数量（遇到文本/错误则报错）。
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

bool SpreadsheetGrid::SetCell(const Address& addr, const string& raw, string* error) {
    // 写入原始值或公式，更新依赖关系。
    if (!addr.is_valid()) {
        if (error) *error = "invalid address";
        return false;
    }

    string normalized_raw = NormalizeFormulaInput(raw);
    int key = Key(addr);
    vector<Address> old_deps;
    auto old_it = cells_.find(key);
    if (old_it != cells_.end()) old_deps = old_it->second.deps;

    if (normalized_raw.empty()) {
        ReplaceDependencies(key, old_deps, {});
        cells_.erase(key);
        MarkDependentsDirty(key);
        return true;
    }

    CellState& cell = cells_[key];
    cell.cell.raw = normalized_raw;
    cell.ast.reset();
    cell.deps.clear();
    cell.formula_error = false;
    cell.eval_error = false;
    cell.dirty = true;

    if (cell.cell.is_formula()) {
        Parser parser(cell.cell.raw.substr(1));
        auto ast = parser.Parse();
        if (!ast) {
            ReplaceDependencies(key, old_deps, {});
            cell.formula_error = true;
            cell.eval_error = false;
            cell.deps.clear();
            cell.cell.value = Value::Number(0.0);
            cell.dirty = false;
            MarkDependentsDirty(key);
            if (error) *error = parser.error().empty() ? "invalid formula" : parser.error();
            return false;
        }

        auto deps = CollectDependencies(ast.get());
        auto get_existing_deps = [this](int current) -> const vector<Address>* {
            auto it = cells_.find(current);
            if (it == cells_.end()) return nullptr;
            return &it->second.deps;
        };
        if (WouldCreateCycle(key, deps, get_existing_deps)) {
            ReplaceDependencies(key, old_deps, {});
            cell.formula_error = true;
            cell.eval_error = false;
            cell.ast.reset();
            cell.deps.clear();
            cell.cell.value = Value::Number(0.0);
            cell.dirty = false;
            MarkDependentsDirty(key);
            if (error) *error = "circular reference";
            return false;
        }

        ReplaceDependencies(key, old_deps, deps);
        cell.ast = move(ast);
        cell.deps = move(deps);
        MarkDependentsDirty(key);
        return true;
    }

    ReplaceDependencies(key, old_deps, {});
    cell.cell.value = ParseRawValue(normalized_raw);
    cell.dirty = false;
    cell.eval_error = false;
    MarkDependentsDirty(key);
    return true;
}

Value SpreadsheetGrid::GetValue(const Address& addr) {
    // 每次查询都走内部缓存逻辑。
    unordered_map<int, int> state;
    return EvalCellInternal(addr, state);
}

EvaluatedCell SpreadsheetGrid::GetEvaluatedCell(const Address& addr) {
    // 同时返回值和错误标记。
    if (!addr.is_valid()) return {};

    unordered_map<int, int> state;
    EvaluatedCell result;
    result.value = EvalCellInternal(addr, state);

    auto it = cells_.find(Key(addr));
    if (it != cells_.end()) {
        result.has_error = it->second.formula_error || it->second.eval_error;
    }
    return result;
}

bool SpreadsheetGrid::HasError(const Address& addr) {
    if (!addr.is_valid()) return false;
    return GetEvaluatedCell(addr).has_error;
}

string SpreadsheetGrid::GetRaw(const Address& addr) const {
    if (!addr.is_valid()) return "";
    int key = Key(addr);
    auto it = cells_.find(key);
    if (it == cells_.end()) return "";
    return it->second.cell.raw;
}

vector<Address> SpreadsheetGrid::CollectAffectedCells(const vector<Address>& roots) const {
    // 从根节点向下游遍历，得到需要刷新的单元格。
    vector<Address> affected;
    vector<int> stack;
    unordered_set<int> visited;
    unordered_set<int> recorded;

    for (const auto& root : roots) {
        if (!root.is_valid()) continue;
        const int key = Key(root);
        if (recorded.insert(key).second) {
            affected.push_back(root);
        }
        stack.push_back(key);
    }

    while (!stack.empty()) {
        const int current = stack.back();
        stack.pop_back();
        if (!visited.insert(current).second) continue;

        auto it = reverse_deps_.find(current);
        if (it == reverse_deps_.end()) continue;

        for (int dependent_key : it->second) {
            if (recorded.insert(dependent_key).second) {
                affected.push_back(Address{dependent_key / kMaxCols, dependent_key % kMaxCols});
            }
            stack.push_back(dependent_key);
        }
    }

    return affected;
}

void SpreadsheetGrid::Clear() {
    // 清空稀疏存储与依赖图。
    cells_.clear();
    reverse_deps_.clear();
}

void SpreadsheetGrid::RecalcAll() {
    // 标记全部 dirty，并强制重新计算一次。
    for (auto& kv : cells_) {
        kv.second.dirty = true;
    }
    unordered_map<int, int> state;
    for (auto& kv : cells_) {
        Address addr{kv.first / kMaxCols, kv.first % kMaxCols};
        EvalCellInternal(addr, state);
    }
}

void SpreadsheetGrid::ForEachCell(const function<void(const Address&, const Cell&)>& fn) const {
    // 遍历所有非空单元格。
    for (const auto& kv : cells_) {
        Address addr{kv.first / kMaxCols, kv.first % kMaxCols};
        fn(addr, kv.second.cell);
    }
}

Value SpreadsheetGrid::EvalCellInternal(const Address& addr, unordered_map<int, int>& state) {
    // 递归求值，state 用于检测循环并复用缓存。
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
            cell.cell.value = Value::Number(0.0);
            cell.eval_error = false;
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
        if (cell.cell.value.is_error()) {
            cell.eval_error = true;
            cell.cell.value = Value::Number(0.0);
        } else {
            cell.eval_error = false;
        }
    } else {
        cell.cell.value = ParseRawValue(cell.cell.raw);
        cell.eval_error = false;
    }

    cell.dirty = false;
    state[key] = 2;
    return cell.cell.value;
}

void SpreadsheetGrid::ReplaceDependencies(int key, const vector<Address>& old_deps, const vector<Address>& new_deps) {
    // 先移除旧依赖，再登记新依赖。
    for (const auto& dep : old_deps) {
        const int dep_key = DependencyKey(dep);
        auto it = reverse_deps_.find(dep_key);
        if (it == reverse_deps_.end()) continue;

        auto& dependents = it->second;
        dependents.erase(remove(dependents.begin(), dependents.end(), key), dependents.end());
        if (dependents.empty()) reverse_deps_.erase(it);
    }

    for (const auto& dep : new_deps) {
        reverse_deps_[DependencyKey(dep)].push_back(key);
    }
}

void SpreadsheetGrid::MarkDependentsDirty(int key) {
    // 从反向依赖表向下游标记 dirty。
    vector<int> stack;
    unordered_set<int> visited;

    auto it = reverse_deps_.find(key);
    if (it == reverse_deps_.end()) return;
    stack = it->second;

    while (!stack.empty()) {
        int current = stack.back();
        stack.pop_back();
        if (!visited.insert(current).second) continue;

        auto cell_it = cells_.find(current);
        if (cell_it != cells_.end()) {
            cell_it->second.dirty = true;
        }

        auto dep_it = reverse_deps_.find(current);
        if (dep_it == reverse_deps_.end()) continue;
        stack.insert(stack.end(), dep_it->second.begin(), dep_it->second.end());
    }
}

}
