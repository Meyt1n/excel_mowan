#include "spreadsheetmodel.h"

#include <QString>
#include <unordered_set>

#include "../core/basic.h"

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
    emw::Value v = grid_.GetValue(addr);
    if (role == Qt::DisplayRole) {
        return QString::fromStdString(v.to_string());
    }
    if (role == Qt::EditRole) {
        std::string raw = grid_.GetRaw(addr);
        return QString::fromStdString(raw);
    }
    if (role == Qt::TextAlignmentRole) {
        if (v.is_number()) {
            return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
        }
        return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
    }
    if (role == Qt::ToolTipRole) {
        const QString raw = QString::fromStdString(grid_.GetRaw(addr));
        const QString display = QString::fromStdString(v.to_string());
        if (!raw.isEmpty() && raw != display) {
            return QString("输入: %1\n结果: %2").arg(raw, display);
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
    std::string text = value.toString().toStdString();
    grid_.SetCell(addr, text, nullptr);
    grid_.RecalcAll();

    emit dataChanged(
        this->index(0, 0),
        this->index(emw::kMaxRows - 1, emw::kMaxCols - 1),
        {Qt::DisplayRole, Qt::EditRole, Qt::ToolTipRole, Qt::TextAlignmentRole}
    );

    return true;
}

bool SpreadsheetModel::setData(const QModelIndexList& indexes, const QVariant& value, int role) {
    if (indexes.isEmpty() || role != Qt::EditRole) return false;

    std::unordered_set<int> seen;
    const std::string text = value.toString().toStdString();
    bool changed = false;
    for (const QModelIndex& index : indexes) {
        if (!index.isValid()) continue;
        const int key = index.row() * emw::kMaxCols + index.column();
        if (!seen.insert(key).second) continue;
        grid_.SetCell(emw::Address{index.row(), index.column()}, text, nullptr);
        changed = true;
    }

    if (!changed) return false;

    grid_.RecalcAll();
    emit dataChanged(
        this->index(0, 0),
        this->index(emw::kMaxRows - 1, emw::kMaxCols - 1),
        {Qt::DisplayRole, Qt::EditRole, Qt::ToolTipRole, Qt::TextAlignmentRole}
    );
    return true;
}

void SpreadsheetModel::clearAll() {
    grid_.Clear();
    emit dataChanged(
        this->index(0, 0),
        this->index(emw::kMaxRows - 1, emw::kMaxCols - 1),
        {Qt::DisplayRole, Qt::EditRole, Qt::ToolTipRole, Qt::TextAlignmentRole}
    );
}

void SpreadsheetModel::recalcAll() {
    grid_.RecalcAll();
    emit dataChanged(
        this->index(0, 0),
        this->index(emw::kMaxRows - 1, emw::kMaxCols - 1),
        {Qt::DisplayRole, Qt::EditRole, Qt::ToolTipRole, Qt::TextAlignmentRole}
    );
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
