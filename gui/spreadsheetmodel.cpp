#include "spreadsheetmodel.h"

#include <algorithm>
#include <QBrush>
#include <QFont>
#include <QString>
#include <unordered_set>

#include "../core/basic.h"

using namespace std;
using std::sort;
using std::unique;


namespace {

const QVector<int>& ChangedRoles() {
    static const QVector<int> roles{
        Qt::DisplayRole,
        Qt::EditRole,
        Qt::ToolTipRole,
        Qt::TextAlignmentRole,
        Qt::ForegroundRole,
        Qt::BackgroundRole,
        Qt::FontRole
    };
    return roles;
}

}  // namespace

bool SpreadsheetModel::CellStyle::IsDefault() const {
    return !has_foreground && !has_background && !bold && !italic && !has_alignment;
}

SpreadsheetModel::SpreadsheetModel(QObject* parent) : QAbstractTableModel(parent) {}

int SpreadsheetModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return emw::kMaxRows;
}

int SpreadsheetModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return emw::kMaxCols;
}

QVariant SpreadsheetModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) return {};

    emw::Address addr{index.row(), index.column()};
    const CellStyle* style = cellStyle(index);
    const emw::EvaluatedCell evaluated = grid_.GetEvaluatedCell(addr);
    const emw::Value& value = evaluated.value;
    const bool has_error = evaluated.has_error;

    if (role == Qt::DisplayRole) {
        if (has_error) return QStringLiteral("#NA");
        return QString::fromStdString(value.to_string());
    }
    if (role == Qt::EditRole) {
        string raw = grid_.GetRaw(addr);
        return QString::fromStdString(raw);
    }
    if (role == Qt::TextAlignmentRole) {
        if (style && style->has_alignment) {
            return static_cast<int>(style->alignment);
        }
        if (!has_error && value.is_number()) {
            return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
        }
        return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
    }
    if (role == Qt::ForegroundRole) {
        if (style && style->has_foreground) {
            return QBrush(style->foreground);
        }
    }
    if (role == Qt::BackgroundRole) {
        if (style && style->has_background) {
            return QBrush(style->background);
        }
    }
    if (role == Qt::FontRole) {
        if (style && (style->bold || style->italic)) {
            QFont font;
            font.setBold(style->bold);
            font.setItalic(style->italic);
            return font;
        }
    }
    if (role == Qt::ToolTipRole) {
        const QString raw = QString::fromStdString(grid_.GetRaw(addr));
        const QString display = has_error ? QStringLiteral("#NA")
                                          : QString::fromStdString(value.to_string());
        if (!raw.isEmpty() && raw != display) {
            return QString("Input: %1\nResult: %2").arg(raw, display);
        }
        if (!display.isEmpty()) {
            return display;
        }
    }
    return {};
}

bool SpreadsheetModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || role != Qt::EditRole) return false;

    emw::Address addr{index.row(), index.column()};
    const vector<emw::Address> roots{addr};
    string text = value.toString().toStdString();
    grid_.SetCell(addr, text, nullptr);
    EmitDataChangedForAddresses(grid_.CollectAffectedCells(roots));

    return true;
}

bool SpreadsheetModel::setData(const QModelIndexList& indexes, const QVariant& value, int role) {
    if (indexes.isEmpty() || role != Qt::EditRole) return false;

    const vector<emw::Address> roots = CollectUniqueAddresses(indexes);
    if (roots.empty()) return false;

    const string text = value.toString().toStdString();
    bool changed = false;
    for (const auto& addr : roots) {
        grid_.SetCell(addr, text, nullptr);
        changed = true;
    }

    if (!changed) return false;

    EmitDataChangedForAddresses(grid_.CollectAffectedCells(roots));
    return true;
}

void SpreadsheetModel::clearAll() {
    beginResetModel();
    grid_.Clear();
    styles_.clear();
    endResetModel();
}

void SpreadsheetModel::recalcAll() {
    const vector<emw::Address> populated = CollectPopulatedAddresses();
    grid_.RecalcAll();
    EmitDataChangedForAddresses(populated);
}

void SpreadsheetModel::loadFromGrid(emw::SpreadsheetGrid&& grid) {
    beginResetModel();
    grid_ = move(grid);
    styles_.clear();
    endResetModel();
}

void SpreadsheetModel::setCellStyle(const QModelIndex& index, const CellStyle& style) {
    if (!index.isValid()) return;

    const emw::Address addr{index.row(), index.column()};
    const int key = AddressKey(addr);
    if (style.IsDefault()) {
        styles_.erase(key);
    } else {
        styles_[key] = style;
    }

    const QVector<int>& roles = ChangedRoles();
    emit dataChanged(index, index, roles);
}

const SpreadsheetModel::CellStyle* SpreadsheetModel::cellStyle(const QModelIndex& index) const {
    if (!index.isValid()) return nullptr;

    const emw::Address addr{index.row(), index.column()};
    const auto it = styles_.find(AddressKey(addr));
    if (it == styles_.end()) return nullptr;
    return &it->second;
}

vector<pair<emw::Address, SpreadsheetModel::CellStyle>> SpreadsheetModel::styledCells() const {
    vector<pair<emw::Address, CellStyle>> out;
    out.reserve(styles_.size());
    for (const auto& entry : styles_) {
        const int key = entry.first;
        out.push_back({emw::Address{key / emw::kMaxCols, key % emw::kMaxCols}, entry.second});
    }
    return out;
}

QVariant SpreadsheetModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole) return {};
    if (orientation == Qt::Horizontal) {
        return ColumnName(section);
    }
    return section + 1;
}

Qt::ItemFlags SpreadsheetModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled;
}

QString SpreadsheetModel::ColumnName(int col) const {
    int c = col;
    QString out;
    do {
        int rem = c % 26;
        out.prepend(QChar('A' + rem));
        c = c / 26 - 1;
    } while (c >= 0);
    return out;
}

int SpreadsheetModel::AddressKey(const emw::Address& addr) const {
    return addr.row * emw::kMaxCols + addr.col;
}

void SpreadsheetModel::EmitDataChangedForAddresses(const vector<emw::Address>& addresses) {
    if (addresses.empty()) return;

    vector<emw::Address> sorted = addresses;
    ::sort(sorted.begin(), sorted.end(), [](const emw::Address& lhs, const emw::Address& rhs) {
        if (lhs.row != rhs.row) return lhs.row < rhs.row;
        return lhs.col < rhs.col;
    });

    sorted.erase(
        ::unique(sorted.begin(), sorted.end(), [](const emw::Address& lhs, const emw::Address& rhs) {
            return lhs.row == rhs.row && lhs.col == rhs.col;
        }),
        sorted.end()
    );

    const QVector<int>& roles = ChangedRoles();
    int segment_row = sorted.front().row;
    int segment_start_col = sorted.front().col;
    int segment_end_col = sorted.front().col;

    auto flush_segment = [this, &roles, &segment_row, &segment_start_col, &segment_end_col]() {
        emit dataChanged(
            index(segment_row, segment_start_col),
            index(segment_row, segment_end_col),
            roles
        );
    };

    for (size_t i = 1; i < sorted.size(); ++i) {
        const emw::Address& addr = sorted[i];
        if (addr.row == segment_row && addr.col == segment_end_col + 1) {
            segment_end_col = addr.col;
            continue;
        }

        flush_segment();
        segment_row = addr.row;
        segment_start_col = addr.col;
        segment_end_col = addr.col;
    }

    flush_segment();
}

vector<emw::Address> SpreadsheetModel::CollectUniqueAddresses(const QModelIndexList& indexes) const {
    vector<emw::Address> addresses;
    unordered_set<int> seen;

    for (const QModelIndex& index : indexes) {
        if (!index.isValid()) continue;
        const emw::Address addr{index.row(), index.column()};
        if (!seen.insert(AddressKey(addr)).second) continue;
        addresses.push_back(addr);
    }

    return addresses;
}

vector<emw::Address> SpreadsheetModel::CollectPopulatedAddresses() const {
    vector<emw::Address> addresses;
    grid_.ForEachCell([&addresses](const emw::Address& addr, const emw::Cell&) {
        addresses.push_back(addr);
    });
    return addresses;
}
