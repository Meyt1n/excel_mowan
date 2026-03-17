#include "dat_file.h"

#include <fstream>
#include <vector>

#include "csv_file.h"
#include "../core/basic.h"

namespace emw {

static bool ParseHeader(const std::vector<std::string>& fields, int* rows, int* cols) {
    if (fields.size() < 4) return false;
    if (fields[0] != "EMW" || fields[1] != "1") return false;
    try {
        *rows = std::stoi(fields[2]);
        *cols = std::stoi(fields[3]);
    } catch (...) {
        return false;
    }
    if (*rows < 0 || *rows > kMaxRows) return false;
    if (*cols < 0 || *cols > kMaxCols) return false;
    return true;
}

bool DatFile::Save(const std::string& path, const SpreadsheetGrid& grid, int rows, int cols) {
    if (rows < 0 || rows > kMaxRows) return false;
    if (cols < 0 || cols > kMaxCols) return false;

    std::ofstream out(path, std::ios::out | std::ios::binary);
    if (!out) return false;

    out << "EMW,1," << rows << "," << cols << "\n";
    grid.ForEachCell([&](const Address& addr, const Cell& cell) {
        if (cell.raw.empty()) return;
        std::vector<std::string> fields;
        fields.push_back(addr.to_string());
        fields.push_back(cell.raw);
        out << CsvFile::JoinLine(fields) << "\n";
    });
    return true;
}

bool DatFile::Load(const std::string& path, SpreadsheetGrid& grid, int* out_rows, int* out_cols, std::string* error) {
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in) {
        if (error) *error = "open failed";
        return false;
    }

    std::string line;
    if (!std::getline(in, line)) {
        if (error) *error = "missing header";
        return false;
    }

    auto header = CsvFile::SplitLine(line);
    int rows = 0;
    int cols = 0;
    if (!ParseHeader(header, &rows, &cols)) {
        if (error) *error = "invalid header";
        return false;
    }

    grid.Clear();
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto fields = CsvFile::SplitLine(line);
        if (fields.size() < 2) continue;
        Address addr;
        if (!Address::TryParse(fields[0], &addr)) continue;
        grid.SetCell(addr, fields[1], nullptr);
    }

    if (out_rows) *out_rows = rows;
    if (out_cols) *out_cols = cols;
    return true;
}

}
