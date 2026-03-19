#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../core/spreadsheet.h"

using namespace std;


namespace emw {

// DAT 中的单元格样式。
struct DatCellStyle {
    bool has_foreground = false;
    uint32_t foreground_rgba = 0;
    bool has_background = false;
    uint32_t background_rgba = 0;

    bool IsDefault() const;
};

// DAT 文档中的稀疏单元格记录。
struct DatCellRecord {
    Address addr;
    string raw;
    DatCellStyle style;
};

// 行高/列宽自定义项。
struct DatSizedSection {
    int index = 0;
    int size = 0;
};

// 合并单元格区域。
struct DatMergeRange {
    Address top_left;
    int row_span = 1;
    int col_span = 1;
};

// DAT 的内存文档模型。
struct DatDocument {
    int rows = 0;
    int cols = 0;
    vector<DatCellRecord> cells;
    vector<DatSizedSection> row_heights;
    vector<DatSizedSection> col_widths;
    vector<DatMergeRange> merged_ranges;
};

class DatFile {
public:
    // 通过运行时网格接口读写。
    static bool Save(const string& path, const SpreadsheetGrid& grid, int rows, int cols);
    static bool Load(const string& path, SpreadsheetGrid& grid, int* out_rows, int* out_cols, string* error = nullptr);

    // 通过文档模型接口读写。
    static bool SaveDocument(const string& path, const DatDocument& document);
    static bool LoadDocument(const string& path, DatDocument& document, string* error = nullptr);
};

}
