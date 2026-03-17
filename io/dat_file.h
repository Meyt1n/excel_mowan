#pragma once

#include <string>

#include "../core/spreadsheet.h"

namespace emw {

class DatFile {
public:
    static bool Save(const std::string& path, const SpreadsheetGrid& grid, int rows, int cols);
    static bool Load(const std::string& path, SpreadsheetGrid& grid, int* out_rows, int* out_cols, std::string* error = nullptr);
};

}
