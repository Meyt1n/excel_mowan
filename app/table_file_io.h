#pragma once

#include <istream>
#include <ostream>
#include <string>
#include <vector>

#include "../core/spreadsheet.h"

namespace emw_app {

bool ReadAllLines(std::istream& in, std::vector<std::string>& lines);
bool LoadCsvToGrid(const std::string& path, emw::SpreadsheetGrid& grid, int& rows, int& cols);
bool LoadCsvLinesToGrid(const std::vector<std::string>& lines, emw::SpreadsheetGrid& grid, int& rows, int& cols);
bool WriteGridValuesCsv(const std::string& path, emw::SpreadsheetGrid& grid, int rows, int cols);
bool WriteGridValuesCsv(std::ostream& out, emw::SpreadsheetGrid& grid, int rows, int cols);

}
