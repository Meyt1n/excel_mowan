#pragma once

#include <istream>
#include <ostream>
#include <string>
#include <vector>

#include "../core/spreadsheet.h"

using namespace std;


namespace emw_app {

// 应用层共享的表格读写辅助接口。

// 从输入流读取全部行。
bool ReadAllLines(istream& in, vector<string>& lines);

// 加载带尺寸头文本（首行 "rows cols"，后续行为 CSV 字段）。
bool LoadInToGrid(const string& path, emw::SpreadsheetGrid& grid, int& rows, int& cols);

// 加载普通 CSV 到表格。
bool LoadCsvToGrid(const string& path, emw::SpreadsheetGrid& grid, int& rows, int& cols);

// 将已读取的 CSV 行解析并填充到表格。
bool LoadCsvLinesToGrid(const vector<string>& lines, emw::SpreadsheetGrid& grid, int& rows, int& cols);

// 从输入流加载带尺寸头文本表格。
bool LoadSizedTextGrid(istream& in, emw::SpreadsheetGrid& grid, int& rows, int& cols);

// 以 CSV 形式导出“计算后的显示值”。
bool WriteGridValuesCsv(const string& path, emw::SpreadsheetGrid& grid, int rows, int cols);
bool WriteGridValuesCsv(ostream& out, emw::SpreadsheetGrid& grid, int rows, int cols);

// 以 CSV 形式导出“用户原始输入”（公式会保留为 =...）。
bool WriteGridRawCsv(const string& path, const emw::SpreadsheetGrid& grid, int rows, int cols);
bool WriteGridRawCsv(ostream& out, const emw::SpreadsheetGrid& grid, int rows, int cols);

// 以纯文本矩阵形式导出“计算后的显示值”。
bool WriteGridValuesPlain(ostream& out, emw::SpreadsheetGrid& grid, int rows, int cols);

// 计算多组 DAT/CSV 文件的平均体积占比（百分比）。
double CalculateStorageEfficiencyPercent(const vector<string>& csv_paths, const vector<string>& dat_paths);

}
