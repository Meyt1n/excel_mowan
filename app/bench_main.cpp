#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "table_file_io.h"

#include "../core/spreadsheet.h"
#include "../io/dat_file.h"

static double CalcAvgMs(const std::vector<double>& values) {
    if (values.empty()) return 0.0;
    double sum = 0.0;
    for (double v : values) sum += v;
    return sum / static_cast<double>(values.size());
}

static std::string ReplaceExtensionWithDat(const std::string& path) {
    size_t slash_pos = path.find_last_of("/\\");
    size_t dot_pos = path.find_last_of('.');
    if (dot_pos == std::string::npos || (slash_pos != std::string::npos && dot_pos < slash_pos)) {
        return path + ".dat";
    }
    return path.substr(0, dot_pos) + ".dat";
}

static bool GetFileSize(const std::string& path, std::uintmax_t& size) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return false;
    std::streampos end = in.tellg();
    if (end < 0) return false;
    size = static_cast<std::uintmax_t>(end);
    return true;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "usage: bench_main.exe case1.csv case2.csv case3.csv\n";
        return 1;
    }

    std::vector<double> times;
    double efficiency_sum = 0.0;

    for (int i = 1; i <= 3; i++) {
        std::string csv_path = argv[i];
        emw::SpreadsheetGrid grid;
        int rows = 0;
        int cols = 0;

        auto t0 = std::chrono::high_resolution_clock::now();
        if (!emw_app::LoadCsvToGrid(csv_path, grid, rows, cols)) {
            std::cerr << "failed to read: " << csv_path << "\n";
            return 1;
        }
        grid.RecalcAll();
        auto t1 = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double, std::milli> ms = t1 - t0;
        times.push_back(ms.count());

        std::string dat_path = ReplaceExtensionWithDat(csv_path);
        if (!emw::DatFile::Save(dat_path, grid, rows, cols)) {
            std::cerr << "failed to save dat: " << dat_path << "\n";
            return 1;
        }

        std::uintmax_t csv_size = 0;
        std::uintmax_t dat_size = 0;
        if (!GetFileSize(csv_path, csv_size) || !GetFileSize(dat_path, dat_size)) {
            std::cerr << "failed to read file size\n";
            return 1;
        }
        if (csv_size == 0) {
            std::cerr << "csv size is zero: " << csv_path << "\n";
            return 1;
        }
        efficiency_sum += (double)dat_size / (double)csv_size;
    }

    double efficiency = (efficiency_sum / 3.0) * 100.0;
    double avg_ms = CalcAvgMs(times);

    std::cout << "avg_time_ms=" << avg_ms << "\n";
    std::cout << "storage_efficiency=" << efficiency << "\n";
    return 0;
}
