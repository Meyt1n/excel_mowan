#include "dat_file.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

#include "csv_file.h"
#include "../core/basic.h"

using namespace std;


namespace emw {

namespace {

constexpr char kBinaryMagic[] = {'E', 'M', 'W', '2'};
constexpr uint16_t kBinaryVersion = 2;

enum CellRecordFlags : uint8_t {
    kHasCachedDisplay = 1 << 0,
    kHasError = 1 << 1,
    kHasForeground = 1 << 2,
    kHasBackground = 1 << 3,
    kBold = 1 << 4,
    kItalic = 1 << 5,
    kHasAlignment = 1 << 6
};

template <typename T>
bool WritePod(ostream& out, const T& value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(T));
    return static_cast<bool>(out);
}

template <typename T>
bool ReadPod(istream& in, T* value) {
    if (!value) return false;
    in.read(reinterpret_cast<char*>(value), sizeof(T));
    return static_cast<bool>(in);
}

bool WriteString(ostream& out, const string& value) {
    const uint32_t size = static_cast<uint32_t>(value.size());
    if (!WritePod(out, size)) return false;
    if (size == 0) return true;
    out.write(value.data(), static_cast<streamsize>(size));
    return static_cast<bool>(out);
}

bool ReadString(istream& in, string* value) {
    if (!value) return false;
    uint32_t size = 0;
    if (!ReadPod(in, &size)) return false;
    value->assign(size, '\0');
    if (size == 0) return true;
    in.read(value->data(), static_cast<streamsize>(size));
    return static_cast<bool>(in);
}

bool ParseLegacyHeader(const vector<string>& fields, int* rows, int* cols) {
    if (fields.size() < 4) return false;
    if (fields[0] != "EMW" || fields[1] != "1") return false;
    try {
        *rows = stoi(fields[2]);
        *cols = stoi(fields[3]);
    } catch (...) {
        return false;
    }
    if (*rows < 0 || *rows > kMaxRows) return false;
    if (*cols < 0 || *cols > kMaxCols) return false;
    return true;
}

bool LoadLegacyDocument(istream& in, DatDocument& document, string* error) {
    string header_line;
    if (!getline(in, header_line)) {
        if (error) *error = "missing header";
        return false;
    }

    const auto header = CsvFile::SplitLine(header_line);
    if (!ParseLegacyHeader(header, &document.rows, &document.cols)) {
        if (error) *error = "invalid header";
        return false;
    }

    document.cells.clear();
    document.row_heights.clear();
    document.col_widths.clear();
    document.merged_ranges.clear();

    string line;
    while (getline(in, line)) {
        if (line.empty()) continue;
        const auto fields = CsvFile::SplitLine(line);
        if (fields.size() < 2) continue;

        Address addr;
        if (!Address::TryParse(fields[0], &addr)) continue;

        DatCellRecord record;
        record.addr = addr;
        record.raw = fields[1];
        if (!record.raw.empty() && record.raw.front() == '=') {
            record.has_cached_display = true;
        }
        document.cells.push_back(move(record));
    }

    return true;
}

bool ValidateDocumentShape(const DatDocument& document) {
    if (document.rows < 0 || document.rows > kMaxRows) return false;
    if (document.cols < 0 || document.cols > kMaxCols) return false;
    return true;
}

bool LoadGridFromDocument(const DatDocument& document, SpreadsheetGrid& grid, int* out_rows, int* out_cols) {
    grid.Clear();
    for (const DatCellRecord& record : document.cells) {
        if (!record.addr.is_valid()) continue;
        grid.SetCell(record.addr, record.raw, nullptr);
    }
    if (out_rows) *out_rows = document.rows;
    if (out_cols) *out_cols = document.cols;
    return true;
}

}  // namespace

bool DatCellStyle::IsDefault() const {
    return !has_foreground && !has_background && !bold && !italic && !has_alignment;
}

bool DatFile::Save(const string& path, const SpreadsheetGrid& grid, int rows, int cols) {
    DatDocument document;
    document.rows = rows;
    document.cols = cols;

    grid.ForEachCell([&](const Address& addr, const Cell& cell) {
        if (cell.raw.empty()) return;

        DatCellRecord record;
        record.addr = addr;
        record.raw = cell.raw;

        if (cell.is_formula()) {
            const EvaluatedCell evaluated = const_cast<SpreadsheetGrid&>(grid).GetEvaluatedCell(addr);
            record.has_cached_display = true;
            record.has_error = evaluated.has_error;
            record.cached_display = evaluated.has_error ? string("#NA") : evaluated.value.to_string();
        }

        document.cells.push_back(move(record));
    });

    return SaveDocument(path, document);
}

bool DatFile::Load(const string& path, SpreadsheetGrid& grid, int* out_rows, int* out_cols, string* error) {
    DatDocument document;
    if (!LoadDocument(path, document, error)) {
        return false;
    }
    return LoadGridFromDocument(document, grid, out_rows, out_cols);
}

bool DatFile::SaveDocument(const string& path, const DatDocument& document) {
    if (!ValidateDocumentShape(document)) return false;

    ofstream out(filesystem::u8path(path), ios::binary | ios::out);
    if (!out) return false;

    // Version 2 stores sparse cell content plus lightweight view metadata,
    // so empty/default cells do not inflate the file size.
    out.write(kBinaryMagic, sizeof(kBinaryMagic));
    if (!WritePod(out, kBinaryVersion)) return false;

    const uint32_t rows = static_cast<uint32_t>(document.rows);
    const uint32_t cols = static_cast<uint32_t>(document.cols);
    const uint32_t cell_count = static_cast<uint32_t>(document.cells.size());
    const uint32_t row_height_count = static_cast<uint32_t>(document.row_heights.size());
    const uint32_t col_width_count = static_cast<uint32_t>(document.col_widths.size());
    const uint32_t merge_count = static_cast<uint32_t>(document.merged_ranges.size());

    if (!WritePod(out, rows) || !WritePod(out, cols) || !WritePod(out, cell_count) ||
        !WritePod(out, row_height_count) || !WritePod(out, col_width_count) || !WritePod(out, merge_count)) {
        return false;
    }

    for (const DatCellRecord& record : document.cells) {
        if (!record.addr.is_valid()) return false;

        const uint16_t row = static_cast<uint16_t>(record.addr.row);
        const uint16_t col = static_cast<uint16_t>(record.addr.col);
        uint8_t flags = 0;
        if (record.has_cached_display) flags |= kHasCachedDisplay;
        if (record.has_error) flags |= kHasError;
        if (record.style.has_foreground) flags |= kHasForeground;
        if (record.style.has_background) flags |= kHasBackground;
        if (record.style.bold) flags |= kBold;
        if (record.style.italic) flags |= kItalic;
        if (record.style.has_alignment) flags |= kHasAlignment;

        if (!WritePod(out, row) || !WritePod(out, col) || !WritePod(out, flags)) return false;
        if (!WriteString(out, record.raw)) return false;
        if (record.has_cached_display && !WriteString(out, record.cached_display)) return false;
        if (record.style.has_foreground && !WritePod(out, record.style.foreground_rgba)) return false;
        if (record.style.has_background && !WritePod(out, record.style.background_rgba)) return false;
        if (record.style.has_alignment && !WritePod(out, record.style.alignment)) return false;
    }

    for (const DatSizedSection& section : document.row_heights) {
        const uint16_t index = static_cast<uint16_t>(section.index);
        const uint32_t size = static_cast<uint32_t>(section.size);
        if (!WritePod(out, index) || !WritePod(out, size)) return false;
    }

    for (const DatSizedSection& section : document.col_widths) {
        const uint16_t index = static_cast<uint16_t>(section.index);
        const uint32_t size = static_cast<uint32_t>(section.size);
        if (!WritePod(out, index) || !WritePod(out, size)) return false;
    }

    for (const DatMergeRange& range : document.merged_ranges) {
        if (!range.top_left.is_valid()) return false;
        const uint16_t row = static_cast<uint16_t>(range.top_left.row);
        const uint16_t col = static_cast<uint16_t>(range.top_left.col);
        const uint16_t row_span = static_cast<uint16_t>(range.row_span);
        const uint16_t col_span = static_cast<uint16_t>(range.col_span);
        if (!WritePod(out, row) || !WritePod(out, col) || !WritePod(out, row_span) || !WritePod(out, col_span)) {
            return false;
        }
    }

    return true;
}

bool DatFile::LoadDocument(const string& path, DatDocument& document, string* error) {
    ifstream in(filesystem::u8path(path), ios::binary | ios::in);
    if (!in) {
        if (error) *error = "open failed";
        return false;
    }

    char magic[sizeof(kBinaryMagic)] = {};
    in.read(magic, sizeof(magic));
    if (!in) {
        if (error) *error = "missing header";
        return false;
    }

    if (equal(magic, magic + sizeof(magic), kBinaryMagic)) {
        uint16_t version = 0;
        uint32_t rows = 0;
        uint32_t cols = 0;
        uint32_t cell_count = 0;
        uint32_t row_height_count = 0;
        uint32_t col_width_count = 0;
        uint32_t merge_count = 0;

        if (!ReadPod(in, &version) || version != kBinaryVersion ||
            !ReadPod(in, &rows) || !ReadPod(in, &cols) || !ReadPod(in, &cell_count) ||
            !ReadPod(in, &row_height_count) || !ReadPod(in, &col_width_count) || !ReadPod(in, &merge_count)) {
            if (error) *error = "invalid binary header";
            return false;
        }

        document = {};
        document.rows = static_cast<int>(rows);
        document.cols = static_cast<int>(cols);
        if (!ValidateDocumentShape(document)) {
            if (error) *error = "invalid document shape";
            return false;
        }

        document.cells.reserve(cell_count);
        for (uint32_t i = 0; i < cell_count; ++i) {
            uint16_t row = 0;
            uint16_t col = 0;
            uint8_t flags = 0;
            if (!ReadPod(in, &row) || !ReadPod(in, &col) || !ReadPod(in, &flags)) {
                if (error) *error = "invalid cell entry";
                return false;
            }

            DatCellRecord record;
            record.addr = Address{static_cast<int>(row), static_cast<int>(col)};
            record.has_cached_display = (flags & kHasCachedDisplay) != 0;
            record.has_error = (flags & kHasError) != 0;
            record.style.has_foreground = (flags & kHasForeground) != 0;
            record.style.has_background = (flags & kHasBackground) != 0;
            record.style.bold = (flags & kBold) != 0;
            record.style.italic = (flags & kItalic) != 0;
            record.style.has_alignment = (flags & kHasAlignment) != 0;

            if (!ReadString(in, &record.raw)) {
                if (error) *error = "invalid cell payload";
                return false;
            }
            if (record.has_cached_display && !ReadString(in, &record.cached_display)) {
                if (error) *error = "invalid cached display";
                return false;
            }
            if (record.style.has_foreground && !ReadPod(in, &record.style.foreground_rgba)) {
                if (error) *error = "invalid foreground style";
                return false;
            }
            if (record.style.has_background && !ReadPod(in, &record.style.background_rgba)) {
                if (error) *error = "invalid background style";
                return false;
            }
            if (record.style.has_alignment && !ReadPod(in, &record.style.alignment)) {
                if (error) *error = "invalid alignment style";
                return false;
            }

            document.cells.push_back(move(record));
        }

        document.row_heights.reserve(row_height_count);
        for (uint32_t i = 0; i < row_height_count; ++i) {
            uint16_t index = 0;
            uint32_t size = 0;
            if (!ReadPod(in, &index) || !ReadPod(in, &size)) {
                if (error) *error = "invalid row size section";
                return false;
            }
            document.row_heights.push_back({static_cast<int>(index), static_cast<int>(size)});
        }

        document.col_widths.reserve(col_width_count);
        for (uint32_t i = 0; i < col_width_count; ++i) {
            uint16_t index = 0;
            uint32_t size = 0;
            if (!ReadPod(in, &index) || !ReadPod(in, &size)) {
                if (error) *error = "invalid column size section";
                return false;
            }
            document.col_widths.push_back({static_cast<int>(index), static_cast<int>(size)});
        }

        document.merged_ranges.reserve(merge_count);
        for (uint32_t i = 0; i < merge_count; ++i) {
            uint16_t row = 0;
            uint16_t col = 0;
            uint16_t row_span = 0;
            uint16_t col_span = 0;
            if (!ReadPod(in, &row) || !ReadPod(in, &col) || !ReadPod(in, &row_span) || !ReadPod(in, &col_span)) {
                if (error) *error = "invalid merge section";
                return false;
            }
            document.merged_ranges.push_back({
                Address{static_cast<int>(row), static_cast<int>(col)},
                static_cast<int>(row_span),
                static_cast<int>(col_span)
            });
        }

        return true;
    }

    // Fall back to the legacy text DAT format so existing files remain readable.
    in.clear();
    in.seekg(0, ios::beg);
    return LoadLegacyDocument(in, document, error);
}

}  // namespace emw
