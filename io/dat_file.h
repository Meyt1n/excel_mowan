#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../core/spreadsheet.h"

using namespace std;


namespace emw {

struct DatCellStyle {
    bool has_foreground = false;
    uint32_t foreground_rgba = 0;
    bool has_background = false;
    uint32_t background_rgba = 0;
    bool bold = false;
    bool italic = false;
    bool has_alignment = false;
    uint32_t alignment = 0;

    bool IsDefault() const;
};

struct DatCellRecord {
    Address addr;
    string raw;
    string cached_display;
    bool has_cached_display = false;
    bool has_error = false;
    DatCellStyle style;
};

struct DatSizedSection {
    int index = 0;
    int size = 0;
};

struct DatMergeRange {
    Address top_left;
    int row_span = 1;
    int col_span = 1;
};

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
    static bool Save(const string& path, const SpreadsheetGrid& grid, int rows, int cols);
    static bool Load(const string& path, SpreadsheetGrid& grid, int* out_rows, int* out_cols, string* error = nullptr);
    static bool SaveDocument(const string& path, const DatDocument& document);
    static bool LoadDocument(const string& path, DatDocument& document, string* error = nullptr);
};

}
