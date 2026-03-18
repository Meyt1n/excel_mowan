#include "table_file_io.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <sstream>

#include "../core/basic.h"
#include "../io/csv_file.h"

using namespace std;


namespace emw_app {

namespace {

void PopulateGrid(const vector<vector<string>>& table, emw::SpreadsheetGrid& grid, int& rows, int& cols) {
    grid.Clear();
    rows = static_cast<int>(table.size());
    cols = rows > 0 ? static_cast<int>(table.front().size()) : 0;

    for (size_t row = 0; row < table.size(); ++row) {
        for (size_t col = 0; col < table[row].size(); ++col) {
            grid.SetCell(emw::Address{static_cast<int>(row), static_cast<int>(col)}, table[row][col], nullptr);
        }
    }
}

string FormatPlainValue(const emw::Value& value) {
    if (value.is_number()) {
        double number = value.number;
        if (fabs(number) < 1e-12) number = 0.0;

        ostringstream oss;
        oss << fixed << setprecision(2) << number;
        return oss.str();
    }
    if (value.is_error() || value.is_empty()) return "0.00";
    return value.to_string();
}

}  // namespace

bool ReadAllLines(istream& in, vector<string>& lines) {
    string line;
    while (getline(in, line)) {
        lines.push_back(line);
    }
    return !lines.empty();
}

bool LoadInToGrid(const string& path, emw::SpreadsheetGrid& grid, int& rows, int& cols) {
    ifstream in(filesystem::u8path(path));
    if (!in) return false;
    return LoadSizedTextGrid(in, grid, rows, cols);
}

bool LoadCsvLinesToGrid(const vector<string>& lines, emw::SpreadsheetGrid& grid, int& rows, int& cols) {
    if (lines.empty()) {
        grid.Clear();
        rows = 0;
        cols = 0;
        return true;
    }

    vector<vector<string>> table;
    size_t max_cols = 0;
    for (const auto& line : lines) {
        auto fields = emw::CsvFile::SplitLine(line);
        max_cols = max(max_cols, fields.size());
        table.push_back(move(fields));
    }

    if (table.size() > (size_t)emw::kMaxRows) table.resize(emw::kMaxRows);
    if (max_cols > (size_t)emw::kMaxCols) max_cols = emw::kMaxCols;
    for (auto& row : table) {
        if (row.size() > max_cols) row.resize(max_cols);
        if (row.size() < max_cols) row.resize(max_cols);
    }

    PopulateGrid(table, grid, rows, cols);
    return true;
}

bool LoadCsvToGrid(const string& path, emw::SpreadsheetGrid& grid, int& rows, int& cols) {
    ifstream fin(filesystem::u8path(path));
    if (!fin) return false;

    vector<string> lines;
    ReadAllLines(fin, lines);
    return LoadCsvLinesToGrid(lines, grid, rows, cols);
}

bool LoadSizedTextGrid(istream& in, emw::SpreadsheetGrid& grid, int& rows, int& cols) {
    string header;
    if (!getline(in, header)) return false;

    // The acceptance data may use either spaces or commas in the first line.
    for (char& ch : header) {
        if (ch == ',' || ch == '\t') ch = ' ';
    }
    istringstream iss(header);
    if (!(iss >> rows >> cols)) return false;
    if (rows < 0 || rows > emw::kMaxRows || cols < 0 || cols > emw::kMaxCols) return false;

    vector<vector<string>> table(static_cast<size_t>(rows), vector<string>(static_cast<size_t>(cols)));
    for (int row = 0; row < rows; ++row) {
        string line;
        if (!getline(in, line)) return false;

        auto fields = emw::CsvFile::SplitLine(line);
        if (fields.size() > static_cast<size_t>(cols)) fields.resize(static_cast<size_t>(cols));
        if (fields.size() < static_cast<size_t>(cols)) fields.resize(static_cast<size_t>(cols));
        table[static_cast<size_t>(row)] = move(fields);
    }

    PopulateGrid(table, grid, rows, cols);
    return true;
}

bool WriteGridValuesCsv(const string& path, emw::SpreadsheetGrid& grid, int rows, int cols) {
    ofstream out(filesystem::u8path(path));
    if (!out) return false;
    return WriteGridValuesCsv(out, grid, rows, cols);
}

bool WriteGridValuesCsv(ostream& out, emw::SpreadsheetGrid& grid, int rows, int cols) {
    for (int r = 0; r < rows; r++) {
        vector<string> fields;
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

bool WriteGridValuesPlain(ostream& out, emw::SpreadsheetGrid& grid, int rows, int cols) {
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            if (col > 0) out << ' ';
            out << FormatPlainValue(grid.GetValue(emw::Address{row, col}));
        }
        out << '\n';
    }
    return true;
}

double CalculateStorageEfficiencyPercent(const vector<string>& csv_paths, const vector<string>& dat_paths) {
    if (csv_paths.empty() || csv_paths.size() != dat_paths.size()) return 0.0;

    double ratio_sum = 0.0;
    int counted = 0;
    for (size_t i = 0; i < csv_paths.size(); ++i) {
        error_code ec1;
        error_code ec2;
        const auto csv_size = filesystem::file_size(csv_paths[i], ec1);
        const auto dat_size = filesystem::file_size(dat_paths[i], ec2);
        if (ec1 || ec2 || csv_size == 0) continue;

        ratio_sum += static_cast<double>(dat_size) / static_cast<double>(csv_size);
        counted++;
    }

    if (counted == 0) return 0.0;
    return ratio_sum / static_cast<double>(counted) * 100.0;
 }

}
