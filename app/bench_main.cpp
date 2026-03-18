#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "table_file_io.h"

#include "../core/spreadsheet.h"
#include "../io/dat_file.h"

using namespace std;


static double CalcAvgMs(const vector<double>& values) {
    if (values.empty()) return 0.0;
    double sum = 0.0;
    for (double v : values) sum += v;
    return sum / static_cast<double>(values.size());
}

static double CalcAvgCount(const vector<size_t>& values) {
    if (values.empty()) return 0.0;
    double sum = 0.0;
    for (size_t v : values) sum += static_cast<double>(v);
    return sum / static_cast<double>(values.size());
}

static string ReplaceExtensionWithDat(const string& path) {
    size_t slash_pos = path.find_last_of("/\\");
    size_t dot_pos = path.find_last_of('.');
    if (dot_pos == string::npos || (slash_pos != string::npos && dot_pos < slash_pos)) {
        return path + ".dat";
    }
    return path.substr(0, dot_pos) + ".dat";
}

static vector<emw::Address> BuildBenchmarkEditPoints(int rows, int cols) {
    vector<emw::Address> points;
    if (rows <= 0 || cols <= 0) return points;

    points.push_back(emw::Address{0, 0});

    const emw::Address middle{rows / 2, cols / 2};
    if (middle.is_valid()) points.push_back(middle);

    const emw::Address last{rows - 1, cols - 1};
    if (last.is_valid()) points.push_back(last);

    vector<emw::Address> unique_points;
    for (const auto& point : points) {
        bool seen = false;
        for (const auto& existing : unique_points) {
            if (existing.row == point.row && existing.col == point.col) {
                seen = true;
                break;
            }
        }
        if (!seen) unique_points.push_back(point);
    }
    return unique_points;
}

static double MeasureIncrementalUpdateMs(
    emw::SpreadsheetGrid& grid,
    int rows,
    int cols,
    vector<size_t>* affected_counts
) {
    const vector<emw::Address> edit_points = BuildBenchmarkEditPoints(rows, cols);
    if (edit_points.empty()) return 0.0;

    vector<double> times;
    for (size_t i = 0; i < edit_points.size(); ++i) {
        const emw::Address addr = edit_points[i];
        const string original_raw = grid.GetRaw(addr);
        const string benchmark_raw = to_string(static_cast<int>(i) + 101);

        const auto t0 = chrono::high_resolution_clock::now();
        grid.SetCell(addr, benchmark_raw, nullptr);
        const vector<emw::Address> affected = grid.CollectAffectedCells({addr});
        for (const auto& affected_addr : affected) {
            grid.GetValue(affected_addr);
        }
        const auto t1 = chrono::high_resolution_clock::now();

        if (affected_counts) affected_counts->push_back(affected.size());
        chrono::duration<double, milli> ms = t1 - t0;
        times.push_back(ms.count());

        grid.SetCell(addr, original_raw, nullptr);
        const vector<emw::Address> restore_affected = grid.CollectAffectedCells({addr});
        for (const auto& affected_addr : restore_affected) {
            grid.GetValue(affected_addr);
        }
    }

    return CalcAvgMs(times);
}

int main(int argc, char** argv) {
    if (argc < 4) {
        cerr << "usage: bench_main.exe case1.csv case2.csv case3.csv\n";
        return 1;
    }

    vector<double> times;
    vector<double> incremental_times;
    vector<size_t> affected_counts;
    vector<string> csv_paths;
    vector<string> dat_paths;

    for (int i = 1; i <= 3; i++) {
        string csv_path = argv[i];
        emw::SpreadsheetGrid grid;
        int rows = 0;
        int cols = 0;

        auto t0 = chrono::high_resolution_clock::now();
        if (!emw_app::LoadCsvToGrid(csv_path, grid, rows, cols)) {
            cerr << "failed to read: " << csv_path << "\n";
            return 1;
        }
        grid.RecalcAll();
        auto t1 = chrono::high_resolution_clock::now();

        chrono::duration<double, milli> ms = t1 - t0;
        times.push_back(ms.count());

        string dat_path = ReplaceExtensionWithDat(csv_path);
        if (!emw::DatFile::Save(dat_path, grid, rows, cols)) {
            cerr << "failed to save dat: " << dat_path << "\n";
            return 1;
        }
        csv_paths.push_back(csv_path);
        dat_paths.push_back(dat_path);

        incremental_times.push_back(MeasureIncrementalUpdateMs(grid, rows, cols, &affected_counts));
    }

    double efficiency = emw_app::CalculateStorageEfficiencyPercent(csv_paths, dat_paths);
    double avg_ms = CalcAvgMs(times);
    double incremental_avg_ms = CalcAvgMs(incremental_times);
    double avg_affected_cells = CalcAvgCount(affected_counts);

    cout << "avg_time_ms=" << avg_ms << "\n";
    cout << "storage_efficiency=" << efficiency << "\n";
    cout << "incremental_update_avg_ms=" << incremental_avg_ms << "\n";
    cout << "incremental_avg_affected_cells=" << avg_affected_cells << "\n";
    return 0;
}
