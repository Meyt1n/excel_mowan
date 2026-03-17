#include <iostream>
#include <string>

#include "table_file_io.h"

#include "../core/spreadsheet.h"
#include "../io/dat_file.h"

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "usage:\n";
        std::cerr << "  dat_tool.exe save input.csv output.dat\n";
        std::cerr << "  dat_tool.exe load input.dat output.csv\n";
        return 1;
    }

    std::string cmd = argv[1];
    std::string in_path = argv[2];
    std::string out_path = argv[3];

    if (cmd == "save") {
        emw::SpreadsheetGrid grid;
        int rows = 0;
        int cols = 0;
        if (!emw_app::LoadCsvToGrid(in_path, grid, rows, cols)) {
            std::cerr << "failed to read csv\n";
            return 1;
        }
        if (!emw::DatFile::Save(out_path, grid, rows, cols)) {
            std::cerr << "failed to write dat\n";
            return 1;
        }
        return 0;
    }

    if (cmd == "load") {
        emw::SpreadsheetGrid grid;
        int rows = 0;
        int cols = 0;
        std::string err;
        if (!emw::DatFile::Load(in_path, grid, &rows, &cols, &err)) {
            std::cerr << "failed to read dat: " << err << "\n";
            return 1;
        }
        grid.RecalcAll();
        if (!emw_app::WriteGridValuesCsv(out_path, grid, rows, cols)) {
            std::cerr << "failed to write csv\n";
            return 1;
        }
        return 0;
    }

    std::cerr << "unknown command: " << cmd << "\n";
    return 1;
}
