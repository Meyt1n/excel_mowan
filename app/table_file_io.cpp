#include "table_file_io.h"

#include <algorithm>
#include <fstream>

#include "../core/basic.h"
#include "../io/csv_file.h"

namespace emw_app {

bool ReadAllLines(std::istream& in, std::vector<std::string>& lines) {
    std::string line;
    while (std::getline(in, line)) {
        lines.push_back(line);
    }
    return !lines.empty();
}

bool LoadCsvLinesToGrid(const std::vector<std::string>& lines, emw::SpreadsheetGrid& grid, int& rows, int& cols) {
    if (lines.empty()) {
        grid.Clear();
        rows = 0;
        cols = 0;
        return true;
    }

    std::vector<std::vector<std::string>> table;
    size_t max_cols = 0;
    for (const auto& line : lines) {
        auto fields = emw::CsvFile::SplitLine(line);
        max_cols = std::max(max_cols, fields.size());
        table.push_back(std::move(fields));
    }

    if (table.size() > (size_t)emw::kMaxRows) table.resize(emw::kMaxRows);
    if (max_cols > (size_t)emw::kMaxCols) max_cols = emw::kMaxCols;

    grid.Clear();
    rows = static_cast<int>(table.size());
    cols = static_cast<int>(max_cols);

    for (size_t r = 0; r < table.size(); r++) {
        auto& row = table[r];
        if (row.size() > max_cols) row.resize(max_cols);
        for (size_t c = 0; c < row.size(); c++) {
            emw::Address addr{static_cast<int>(r), static_cast<int>(c)};
            grid.SetCell(addr, row[c], nullptr);
        }
    }

    return true;
}

bool LoadCsvToGrid(const std::string& path, emw::SpreadsheetGrid& grid, int& rows, int& cols) {
    std::ifstream fin(path);
    if (!fin) return false;

    std::vector<std::string> lines;
    ReadAllLines(fin, lines);
    return LoadCsvLinesToGrid(lines, grid, rows, cols);
}

bool WriteGridValuesCsv(const std::string& path, emw::SpreadsheetGrid& grid, int rows, int cols) {
    std::ofstream out(path);
    if (!out) return false;
    return WriteGridValuesCsv(out, grid, rows, cols);
}

bool WriteGridValuesCsv(std::ostream& out, emw::SpreadsheetGrid& grid, int rows, int cols) {
    for (int r = 0; r < rows; r++) {
        std::vector<std::string> fields;
        fields.reserve(cols);
        for (int c = 0; c < cols; c++) {
            emw::Address addr{r, c};
            emw::Value v = grid.GetValue(addr);
            fields.push_back(v.to_string());
        }
        out << emw::CsvFile::JoinLine(fields) << "\n";
    }
    return true;
}

}
