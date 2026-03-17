#pragma once

#include <QAbstractTableModel>
#include <QModelIndexList>

#include "../core/spreadsheet.h"

class SpreadsheetModel : public QAbstractTableModel {
public:
    explicit SpreadsheetModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    bool setData(const QModelIndexList& indexes, const QVariant& value, int role = Qt::EditRole);
    void clearAll();
    void recalcAll();

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    emw::SpreadsheetGrid& grid() { return grid_; }
    const emw::SpreadsheetGrid& grid() const { return grid_; }

private:
    QString ColumnName(int col) const;

    mutable emw::SpreadsheetGrid grid_;
};
