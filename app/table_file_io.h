#pragma once

#include <istream>
#include <ostream>
#include <string>
#include <vector>

#include "../core/spreadsheet.h"

using namespace std;


namespace emw_app {

bool ReadAllLines(istream& in, vector<string>& lines);
bool LoadInToGrid(const string& path, emw::SpreadsheetGrid& grid, int& rows, int& cols);
bool LoadCsvToGrid(const string& path, emw::SpreadsheetGrid& grid, int& rows, int& cols);
bool LoadCsvLinesToGrid(const vector<string>& lines, emw::SpreadsheetGrid& grid, int& rows, int& cols);
bool LoadSizedTextGrid(istream& in, emw::SpreadsheetGrid& grid, int& rows, int& cols);
bool WriteGridValuesCsv(const string& path, emw::SpreadsheetGrid& grid, int rows, int cols);
bool WriteGridValuesCsv(ostream& out, emw::SpreadsheetGrid& grid, int rows, int cols);
bool WriteGridValuesPlain(ostream& out, emw::SpreadsheetGrid& grid, int rows, int cols);
double CalculateStorageEfficiencyPercent(const vector<string>& csv_paths, const vector<string>& dat_paths);

}
