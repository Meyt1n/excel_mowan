#include <iostream>
#include <string>

#include "table_file_io.h"

#include "../core/spreadsheet.h"
#include "dat_file.h"

using namespace std;


int main(int argc, char** argv) {
    // 命令行转换工具：支持 CSV -> DAT 与 DAT -> CSV。
    if (argc < 4) {
        cerr << "usage:\n";
        cerr << "  dat_tool.exe save input.csv output.dat\n";
        cerr << "  dat_tool.exe load input.dat output.csv\n";
        return 1;
    }

    string cmd = argv[1];       
    string in_path = argv[2];
    string out_path = argv[3];

    if (cmd == "save") {
        // 读取 CSV，按当前 DAT 存储策略写出。
        emw::SpreadsheetGrid grid;
        int rows = 0;
        int cols = 0;
        if (!emw_app::LoadCsvToGrid(in_path, grid, rows, cols)) {
            cerr << "failed to read csv\n";
            return 1;
        }
        if (!emw::DatFile::Save(out_path, grid, rows, cols)) {
            cerr << "failed to write dat\n";
            return 1;
        }
        return 0;
    }

    if (cmd == "load") {
        // 读取 DAT，重算后导出为值视图 CSV。
        emw::SpreadsheetGrid grid;
        int rows = 0;
        int cols = 0;
        string err;
        if (!emw::DatFile::Load(in_path, grid, &rows, &cols, &err)) {
            cerr << "failed to read dat: " << err << "\n";
            return 1;
        }
        grid.RecalcAll();
        if (!emw_app::WriteGridValuesCsv(out_path, grid, rows, cols)) {
            cerr << "failed to write csv\n";
            return 1;
        }
        return 0;
    }

    cerr << "unknown command: " << cmd << "\n";
    return 1;
}
