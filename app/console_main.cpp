#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "table_file_io.h"

#include "../core/spreadsheet.h"

int main(int argc, char** argv) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::vector<std::string> lines;
    if (argc >= 2) {
        std::ifstream fin(argv[1]);
        if (!fin) {
            std::cerr << "failed to open input file\n";
            return 1;
        }
        emw_app::ReadAllLines(fin, lines);
    } else {
        emw_app::ReadAllLines(std::cin, lines);
    }

    if (lines.empty()) {
        return 0;
    }

    emw::SpreadsheetGrid grid;
    int rows = 0;
    int cols = 0;
    if (!emw_app::LoadCsvLinesToGrid(lines, grid, rows, cols)) {
        std::cerr << "failed to parse csv\n";
        return 1;
    }

    auto start = std::chrono::high_resolution_clock::now();
    grid.RecalcAll();
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> ms = end - start;

    if (argc >= 3) {
        if (!emw_app::WriteGridValuesCsv(argv[2], grid, rows, cols)) {
            std::cerr << "failed to open output file\n";
            return 1;
        }
    } else {
        emw_app::WriteGridValuesCsv(std::cout, grid, rows, cols);
    }

    std::cerr << "calc_ms=" << ms.count() << "\n";
    return 0;
}
