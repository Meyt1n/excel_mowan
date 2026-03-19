#pragma once

#include <QAbstractTableModel>
#include <QColor>
#include <QModelIndexList>

#include <unordered_map>
#include <utility>
#include <vector>

#include "../core/spreadsheet.h"

using namespace std;


// SpreadsheetGrid 的 Qt 模型适配层，支持增量刷新。
class SpreadsheetModel : public QAbstractTableModel {
public:
    struct CellStyle {
        bool has_foreground = false;
        QColor foreground;
        bool has_background = false;
        QColor background;

        bool IsDefault() const;
    };

    explicit SpreadsheetModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    bool setData(const QModelIndexList& indexes, const QVariant& value, int role = Qt::EditRole);
    void clearAll();
    void recalcAll();
    void loadFromGrid(emw::SpreadsheetGrid&& grid);
    void loadFromGrid(emw::SpreadsheetGrid&& grid, unordered_map<int, CellStyle>&& styles);
    void setCellStyle(const QModelIndex& index, const CellStyle& style);
    const CellStyle* cellStyle(const QModelIndex& index) const;
    vector<pair<emw::Address, CellStyle>> styledCells() const;

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    emw::SpreadsheetGrid& grid() { return grid_; }
    const emw::SpreadsheetGrid& grid() const { return grid_; }

private:
    QString ColumnName(int col) const;
    void EmitDataChangedForAddresses(const vector<emw::Address>& addresses);
    vector<emw::Address> CollectUniqueAddresses(const QModelIndexList& indexes) const;
    vector<emw::Address> CollectPopulatedAddresses() const;
    int AddressKey(const emw::Address& addr) const;

    mutable emw::SpreadsheetGrid grid_;
    unordered_map<int, CellStyle> styles_;
};
