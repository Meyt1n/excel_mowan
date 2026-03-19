#include "dat_file.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "csv_file.h"
#include "../core/basic.h"

using namespace std;


namespace emw {

namespace {

constexpr char kBinaryMagicV4[] = {'E', 'M', 'W', '4'};
constexpr char kBinaryMagicZ[] = {'E', 'M', 'W', 'Z'};

constexpr uint16_t kBinaryVersionV4 = 4;
constexpr uint16_t kBinaryVersionZ = 1;

enum CellRecordFlags : uint8_t {
    kHasForeground = 1 << 0,
    kHasBackground = 1 << 1
};

enum DatStorageMode : uint8_t {
    kSparseDocument = 0
};

enum DatCompressionCodec : uint8_t {
    kCompressionLzss = 1
};

constexpr size_t kLzssWindowSize = 4095;
constexpr size_t kLzssMinMatch = 3;
constexpr size_t kLzssMaxMatch = 18;

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

bool WriteVarUInt32(ostream& out, uint32_t value) {
    while (value >= 0x80U) {
        const uint8_t byte = static_cast<uint8_t>(value) | 0x80U;
        if (!WritePod(out, byte)) return false;
        value >>= 7U;
    }
    return WritePod(out, static_cast<uint8_t>(value));
}

bool ReadVarUInt32(istream& in, uint32_t* value) {
    if (!value) return false;

    uint32_t result = 0;
    uint32_t shift = 0;
    for (int i = 0; i < 5; ++i) {
        uint8_t byte = 0;
        if (!ReadPod(in, &byte)) return false;

        result |= static_cast<uint32_t>(byte & 0x7FU) << shift;
        if ((byte & 0x80U) == 0) {
            *value = result;
            return true;
        }
        shift += 7U;
    }
    return false;
}

bool WriteSizedString(ostream& out, const string& value) {
    if (!WriteVarUInt32(out, static_cast<uint32_t>(value.size()))) return false;
    if (value.empty()) return true;
    out.write(value.data(), static_cast<streamsize>(value.size()));
    return static_cast<bool>(out);
}

bool ReadSizedString(istream& in, string* value) {
    if (!value) return false;

    uint32_t size = 0;
    if (!ReadVarUInt32(in, &size)) return false;
    value->assign(size, '\0');
    if (size == 0) return true;
    in.read(value->data(), static_cast<streamsize>(size));
    return static_cast<bool>(in);
}

vector<uint8_t> LzssCompress(const vector<uint8_t>& input) {
    vector<uint8_t> output;
    if (input.empty()) return output;

    output.reserve(input.size());
    size_t cursor = 0;
    while (cursor < input.size()) {
        const size_t flag_offset = output.size();
        output.push_back(0);
        uint8_t flags = 0;

        for (int bit = 0; bit < 8 && cursor < input.size(); ++bit) {
            size_t best_len = 0;
            size_t best_dist = 0;

            const size_t search_begin = cursor > kLzssWindowSize ? cursor - kLzssWindowSize : 0;
            for (size_t probe = cursor; probe > search_begin; --probe) {
                const size_t pos = probe - 1;
                if (input[pos] != input[cursor]) continue;

                const size_t max_len = min(kLzssMaxMatch, input.size() - cursor);
                size_t len = 1;
                while (len < max_len && input[pos + len] == input[cursor + len]) {
                    ++len;
                }
                if (len >= kLzssMinMatch && len > best_len) {
                    best_len = len;
                    best_dist = cursor - pos;
                    if (best_len == kLzssMaxMatch) break;
                }
            }

            if (best_len >= kLzssMinMatch) {
                const uint16_t token =
                    static_cast<uint16_t>(((best_dist - 1U) << 4U) | (best_len - kLzssMinMatch));
                output.push_back(static_cast<uint8_t>(token & 0xFFU));
                output.push_back(static_cast<uint8_t>((token >> 8U) & 0xFFU));
                cursor += best_len;
            } else {
                flags |= static_cast<uint8_t>(1U << bit);
                output.push_back(input[cursor]);
                ++cursor;
            }
        }

        output[flag_offset] = flags;
    }

    return output;
}

bool LzssDecompress(const vector<uint8_t>& input, uint32_t expected_size, vector<uint8_t>* output) {
    if (!output) return false;
    output->clear();
    output->reserve(expected_size);

    size_t cursor = 0;
    while (cursor < input.size() && output->size() < expected_size) {
        const uint8_t flags = input[cursor++];
        for (int bit = 0; bit < 8 && output->size() < expected_size; ++bit) {
            const bool literal = (flags & static_cast<uint8_t>(1U << bit)) != 0;
            if (literal) {
                if (cursor >= input.size()) return false;
                output->push_back(input[cursor++]);
                continue;
            }

            if (cursor + 1 >= input.size()) return false;
            const uint16_t token =
                static_cast<uint16_t>(input[cursor]) |
                static_cast<uint16_t>(static_cast<uint16_t>(input[cursor + 1]) << 8U);
            cursor += 2;

            const size_t distance = static_cast<size_t>((token >> 4U) + 1U);
            const size_t length = static_cast<size_t>((token & 0x0FU) + kLzssMinMatch);
            if (distance == 0 || distance > output->size()) return false;
            if (output->size() + length > expected_size) return false;

            const size_t copy_from = output->size() - distance;
            for (size_t i = 0; i < length; ++i) {
                output->push_back((*output)[copy_from + i]);
            }
        }
    }

    return cursor == input.size() && output->size() == expected_size;
}

struct StyleKey {
    bool has_foreground = false;
    uint32_t foreground_rgba = 0;
    bool has_background = false;
    uint32_t background_rgba = 0;
};

bool operator==(const StyleKey& lhs, const StyleKey& rhs) {
    return lhs.has_foreground == rhs.has_foreground &&
           lhs.foreground_rgba == rhs.foreground_rgba &&
           lhs.has_background == rhs.has_background &&
           lhs.background_rgba == rhs.background_rgba;
}

struct StyleKeyHasher {
    size_t operator()(const StyleKey& key) const {
        size_t hash = 0;
        auto combine = [&hash](uint64_t value) {
            hash ^= static_cast<size_t>(value) + 0x9e3779b9U + (hash << 6U) + (hash >> 2U);
        };
        combine(key.has_foreground ? 1U : 0U);
        combine(key.foreground_rgba);
        combine(key.has_background ? 1U : 0U);
        combine(key.background_rgba);
        return hash;
    }
};

StyleKey MakeStyleKey(const DatCellStyle& style) {
    StyleKey key;
    key.has_foreground = style.has_foreground;
    key.foreground_rgba = style.has_foreground ? style.foreground_rgba : 0U;
    key.has_background = style.has_background;
    key.background_rgba = style.has_background ? style.background_rgba : 0U;
    return key;
}

bool WriteStyle(ostream& out, const DatCellStyle& style) {
    uint8_t flags = 0;
    if (style.has_foreground) flags |= kHasForeground;
    if (style.has_background) flags |= kHasBackground;

    if (!WritePod(out, flags)) return false;
    if (style.has_foreground && !WritePod(out, style.foreground_rgba)) return false;
    if (style.has_background && !WritePod(out, style.background_rgba)) return false;
    return true;
}

bool ReadStyle(istream& in, DatCellStyle* style) {
    if (!style) return false;

    uint8_t flags = 0;
    if (!ReadPod(in, &flags)) return false;

    *style = {};
    style->has_foreground = (flags & kHasForeground) != 0;
    style->has_background = (flags & kHasBackground) != 0;

    if (style->has_foreground && !ReadPod(in, &style->foreground_rgba)) return false;
    if (style->has_background && !ReadPod(in, &style->background_rgba)) return false;
    return true;
}

void BuildStyleTable(const DatDocument& document, vector<DatCellStyle>* styles, vector<uint32_t>* style_ids) {
    if (!styles || !style_ids) return;

    styles->clear();
    style_ids->clear();
    styles->push_back({});

    unordered_map<StyleKey, uint32_t, StyleKeyHasher> style_to_id;
    style_to_id.emplace(MakeStyleKey(styles->front()), 0U);

    style_ids->reserve(document.cells.size());
    for (const DatCellRecord& record : document.cells) {
        const StyleKey key = MakeStyleKey(record.style);
        const auto it = style_to_id.find(key);
        if (it != style_to_id.end()) {
            style_ids->push_back(it->second);
            continue;
        }

        const uint32_t id = static_cast<uint32_t>(styles->size());
        styles->push_back(record.style);
        style_to_id.emplace(key, id);
        style_ids->push_back(id);
    }
}

void SortDocumentCells(DatDocument* document) {
    if (!document) return;
    sort(document->cells.begin(), document->cells.end(), [](const DatCellRecord& lhs, const DatCellRecord& rhs) {
        if (lhs.addr.row != rhs.addr.row) return lhs.addr.row < rhs.addr.row;
        return lhs.addr.col < rhs.addr.col;
    });
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

bool SaveV4Sparse(ostream& out, const DatDocument& document) {
    vector<DatCellStyle> styles;
    vector<uint32_t> style_ids;
    BuildStyleTable(document, &styles, &style_ids);
    if (style_ids.size() != document.cells.size()) return false;

    out.write(kBinaryMagicV4, sizeof(kBinaryMagicV4));
    if (!WritePod(out, kBinaryVersionV4)) return false;

    const uint8_t mode = kSparseDocument;
    const uint32_t rows = static_cast<uint32_t>(document.rows);
    const uint32_t cols = static_cast<uint32_t>(document.cols);
    const uint32_t cell_count = static_cast<uint32_t>(document.cells.size());
    const uint32_t style_count = static_cast<uint32_t>(styles.size());
    const uint32_t row_height_count = static_cast<uint32_t>(document.row_heights.size());
    const uint32_t col_width_count = static_cast<uint32_t>(document.col_widths.size());
    const uint32_t merge_count = static_cast<uint32_t>(document.merged_ranges.size());
    if (!WritePod(out, mode) || !WritePod(out, rows) || !WritePod(out, cols) ||
        !WritePod(out, cell_count) || !WritePod(out, style_count) ||
        !WritePod(out, row_height_count) || !WritePod(out, col_width_count) || !WritePod(out, merge_count)) {
        return false;
    }

    for (const DatCellStyle& style : styles) {
        if (!WriteStyle(out, style)) return false;
    }
    for (size_t i = 0; i < document.cells.size(); ++i) {
        const DatCellRecord& record = document.cells[i];
        if (!record.addr.is_valid()) return false;
        if (!WriteVarUInt32(out, static_cast<uint32_t>(record.addr.row)) ||
            !WriteVarUInt32(out, static_cast<uint32_t>(record.addr.col)) ||
            !WriteVarUInt32(out, style_ids[i])) return false;
        if (!WriteSizedString(out, record.raw)) return false;
    }
    for (const DatSizedSection& section : document.row_heights) {
        if (section.index < 0 || section.size < 0) return false;
        if (!WriteVarUInt32(out, static_cast<uint32_t>(section.index)) ||
            !WriteVarUInt32(out, static_cast<uint32_t>(section.size))) return false;
    }
    for (const DatSizedSection& section : document.col_widths) {
        if (section.index < 0 || section.size < 0) return false;
        if (!WriteVarUInt32(out, static_cast<uint32_t>(section.index)) ||
            !WriteVarUInt32(out, static_cast<uint32_t>(section.size))) return false;
    }
    for (const DatMergeRange& range : document.merged_ranges) {
        if (!range.top_left.is_valid() || range.row_span < 1 || range.col_span < 1) return false;
        if (!WriteVarUInt32(out, static_cast<uint32_t>(range.top_left.row)) ||
            !WriteVarUInt32(out, static_cast<uint32_t>(range.top_left.col)) ||
            !WriteVarUInt32(out, static_cast<uint32_t>(range.row_span)) ||
            !WriteVarUInt32(out, static_cast<uint32_t>(range.col_span))) return false;
    }
    return true;
}

bool LoadV4(istream& in, DatDocument& document, string* error) {
    uint16_t version = 0;
    uint8_t mode = kSparseDocument;
    uint32_t rows = 0;
    uint32_t cols = 0;
    if (!ReadPod(in, &version) || version != kBinaryVersionV4 ||
        !ReadPod(in, &mode) || !ReadPod(in, &rows) || !ReadPod(in, &cols)) {
        if (error) *error = "invalid binary header";
        return false;
    }
    if (mode != kSparseDocument) {
        if (error) *error = "unknown storage mode";
        return false;
    }

    uint32_t cell_count = 0;
    uint32_t style_count = 0;
    uint32_t row_height_count = 0;
    uint32_t col_width_count = 0;
    uint32_t merge_count = 0;
    if (!ReadPod(in, &cell_count) || !ReadPod(in, &style_count) ||
        !ReadPod(in, &row_height_count) || !ReadPod(in, &col_width_count) || !ReadPod(in, &merge_count)) {
        if (error) *error = "invalid binary header";
        return false;
    }
    if (style_count == 0) {
        if (error) *error = "missing style table";
        return false;
    }

    document = {};
    document.rows = static_cast<int>(rows);
    document.cols = static_cast<int>(cols);
    if (!ValidateDocumentShape(document)) {
        if (error) *error = "invalid document shape";
        return false;
    }

    vector<DatCellStyle> styles;
    styles.reserve(style_count);
    for (uint32_t i = 0; i < style_count; ++i) {
        DatCellStyle style;
        if (!ReadStyle(in, &style)) {
            if (error) *error = "invalid style table";
            return false;
        }
        styles.push_back(style);
    }

    document.cells.reserve(cell_count);
    for (uint32_t i = 0; i < cell_count; ++i) {
        uint32_t row = 0;
        uint32_t col = 0;
        uint32_t style_id = 0;
        if (!ReadVarUInt32(in, &row) || !ReadVarUInt32(in, &col) || !ReadVarUInt32(in, &style_id)) {
            if (error) *error = "invalid cell entry";
            return false;
        }
        if (style_id >= styles.size()) {
            if (error) *error = "invalid style reference";
            return false;
        }

        DatCellRecord record;
        record.addr = Address{static_cast<int>(row), static_cast<int>(col)};
        record.style = styles[style_id];
        if (!ReadSizedString(in, &record.raw)) {
            if (error) *error = "invalid cell payload";
            return false;
        }
        document.cells.push_back(move(record));
    }

    document.row_heights.reserve(row_height_count);
    for (uint32_t i = 0; i < row_height_count; ++i) {
        uint32_t index = 0;
        uint32_t size = 0;
        if (!ReadVarUInt32(in, &index) || !ReadVarUInt32(in, &size)) {
            if (error) *error = "invalid row size section";
            return false;
        }
        document.row_heights.push_back({static_cast<int>(index), static_cast<int>(size)});
    }
    document.col_widths.reserve(col_width_count);
    for (uint32_t i = 0; i < col_width_count; ++i) {
        uint32_t index = 0;
        uint32_t size = 0;
        if (!ReadVarUInt32(in, &index) || !ReadVarUInt32(in, &size)) {
            if (error) *error = "invalid column size section";
            return false;
        }
        document.col_widths.push_back({static_cast<int>(index), static_cast<int>(size)});
    }
    document.merged_ranges.reserve(merge_count);
    for (uint32_t i = 0; i < merge_count; ++i) {
        uint32_t row = 0;
        uint32_t col = 0;
        uint32_t row_span = 0;
        uint32_t col_span = 0;
        if (!ReadVarUInt32(in, &row) || !ReadVarUInt32(in, &col) ||
            !ReadVarUInt32(in, &row_span) || !ReadVarUInt32(in, &col_span)) {
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

bool DispatchLoadByMagic(const char* magic, istream& in, DatDocument& document, string* error) {
    if (equal(magic, magic + sizeof(kBinaryMagicV4), kBinaryMagicV4)) return LoadV4(in, document, error);
    if (error) *error = "unsupported dat format";
    return false;
}

bool LoadFromBuffer(const vector<uint8_t>& encoded, DatDocument& document, string* error) {
    if (encoded.size() < sizeof(kBinaryMagicV4)) {
        if (error) *error = "missing header";
        return false;
    }

    char magic[sizeof(kBinaryMagicV4)] = {};
    copy_n(encoded.begin(), sizeof(magic), magic);
    string payload(encoded.begin() + sizeof(magic), encoded.end());
    istringstream in(payload, ios::binary | ios::in);
    return DispatchLoadByMagic(magic, in, document, error);
}

bool LoadCompressed(istream& in, DatDocument& document, string* error) {
    uint16_t version = 0;
    uint8_t codec = 0;
    uint32_t raw_size = 0;
    uint32_t compressed_size = 0;
    if (!ReadPod(in, &version) || !ReadPod(in, &codec) ||
        !ReadPod(in, &raw_size) || !ReadPod(in, &compressed_size)) {
        if (error) *error = "invalid compressed header";
        return false;
    }
    if (version != kBinaryVersionZ) {
        if (error) *error = "unsupported compressed version";
        return false;
    }

    vector<uint8_t> compressed(compressed_size);
    if (compressed_size > 0) {
        in.read(reinterpret_cast<char*>(compressed.data()), static_cast<streamsize>(compressed.size()));
        if (!in) {
            if (error) *error = "truncated compressed payload";
            return false;
        }
    }

    vector<uint8_t> decoded;
    if (codec == kCompressionLzss) {
        if (!LzssDecompress(compressed, raw_size, &decoded)) {
            if (error) *error = "invalid compressed payload";
            return false;
        }
    } else {
        if (error) *error = "unknown compression codec";
        return false;
    }

    return LoadFromBuffer(decoded, document, error);
}

bool WriteMaybeCompressed(ostream& out, const vector<uint8_t>& encoded) {
    const vector<uint8_t> compressed = LzssCompress(encoded);
    const uint64_t header_size =
        sizeof(kBinaryMagicZ) + sizeof(uint16_t) + sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint32_t);
    const bool should_compress =
        !encoded.empty() && (static_cast<uint64_t>(compressed.size()) + header_size < encoded.size());

    if (!should_compress) {
        if (!encoded.empty()) {
            out.write(reinterpret_cast<const char*>(encoded.data()), static_cast<streamsize>(encoded.size()));
        }
        return static_cast<bool>(out);
    }

    out.write(kBinaryMagicZ, sizeof(kBinaryMagicZ));
    if (!WritePod(out, kBinaryVersionZ)) return false;
    const uint8_t codec = kCompressionLzss;
    const uint32_t raw_size = static_cast<uint32_t>(encoded.size());
    const uint32_t compressed_size = static_cast<uint32_t>(compressed.size());
    if (!WritePod(out, codec) || !WritePod(out, raw_size) || !WritePod(out, compressed_size)) return false;
    if (compressed_size > 0) {
        out.write(reinterpret_cast<const char*>(compressed.data()), static_cast<streamsize>(compressed.size()));
    }
    return static_cast<bool>(out);
}

bool ParseLegacyHeader(const vector<string>& fields, int* rows, int* cols) {
    if (!rows || !cols) return false;
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

bool LoadLegacyText(istream& in, DatDocument& document, string* error) {
    document = {};
    string header_line;
    if (!getline(in, header_line)) {
        if (error) *error = "missing header";
        return false;
    }
    const vector<string> header = CsvFile::SplitLine(header_line);
    if (!ParseLegacyHeader(header, &document.rows, &document.cols)) {
        if (error) *error = "invalid header";
        return false;
    }

    string line;
    while (getline(in, line)) {
        if (line.empty()) continue;
        const vector<string> fields = CsvFile::SplitLine(line);
        if (fields.size() < 2) continue;
        Address addr;
        if (!Address::TryParse(fields[0], &addr)) continue;
        DatCellRecord record;
        record.addr = addr;
        record.raw = fields[1];
        document.cells.push_back(move(record));
    }
    return true;
}

}  // namespace

bool DatCellStyle::IsDefault() const {
    return !has_foreground && !has_background;
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
        document.cells.push_back(move(record));
    });

    return SaveDocument(path, document);
}

bool DatFile::Load(const string& path, SpreadsheetGrid& grid, int* out_rows, int* out_cols, string* error) {
    DatDocument document;
    if (!LoadDocument(path, document, error)) return false;
    return LoadGridFromDocument(document, grid, out_rows, out_cols);
}

bool DatFile::SaveDocument(const string& path, const DatDocument& document) {
    if (!ValidateDocumentShape(document)) return false;

    DatDocument normalized = document;
    SortDocumentCells(&normalized);

    ostringstream payload(ios::binary | ios::out);
    if (!SaveV4Sparse(payload, normalized)) return false;

    const string payload_bytes = payload.str();
    vector<uint8_t> encoded(payload_bytes.begin(), payload_bytes.end());

    ofstream out(filesystem::u8path(path), ios::binary | ios::out);
    if (!out) return false;
    return WriteMaybeCompressed(out, encoded);
}

bool DatFile::LoadDocument(const string& path, DatDocument& document, string* error) {
    ifstream in(filesystem::u8path(path), ios::binary | ios::in);
    if (!in) {
        if (error) *error = "open failed";
        return false;
    }

    char magic[sizeof(kBinaryMagicV4)] = {};
    in.read(magic, sizeof(magic));
    if (!in) {
        if (error) *error = "missing header";
        return false;
    }

    if (equal(magic, magic + sizeof(magic), kBinaryMagicZ)) {
        return LoadCompressed(in, document, error);
    }
    if (equal(magic, magic + sizeof(magic), kBinaryMagicV4)) {
        return DispatchLoadByMagic(magic, in, document, error);
    }

    // Fallback for old text DAT.
    in.clear();
    in.seekg(0, ios::beg);
    return LoadLegacyText(in, document, error);
}

}  // namespace emw
