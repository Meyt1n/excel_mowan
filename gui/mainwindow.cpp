#include "mainwindow.h"

#include <algorithm>

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCollator>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QEasingCurve>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QProcess>
#include <QSizePolicy>
#include <QStandardPaths>
#include <QStatusBar>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QStyle>
#include <QTableView>
#include <QStringList>
#include <QTimer>
#include <QToolBar>
#include <QUrl>
#include <QVector>
#include <QVBoxLayout>
#include <QWidget>
#include <QtConcurrent/QtConcurrentRun>
#include <cmath>
#include <memory>
#include <set>
#include <unordered_map>

#include "../io/table_file_io.h"
#include "../core/basic.h"
#include "spreadsheetmodel.h"
#include "spreadsheetview.h"

using namespace std;
using std::move;


namespace {

constexpr int kDefaultRowHeight = 34;
constexpr int kDefaultColumnWidth = 126;
constexpr int kCellResizeStep = 18;

QString DefaultFormulaPlaceholder() {
    return QStringLiteral("\u8f93\u5165\u6570\u5b57\u3001\u6587\u672c\u6216\u516c\u5f0f\uff0c\u4f8b\u5982 =SUM(A1:B3)");
}

QString BuildHelpGuideText() {
    return QString::fromUtf8(u8R"(【一、基础输入】
1. 直接输入数字：例如 12、3.14。
2. 直接输入文本：例如 hello、姓名。
3. 输入公式：必须以 = 开头，例如 =A1+B1。
4. 选区引用：公式编辑状态下点击单元格，会自动插入地址（如 A1 或 A1:B3）。

【二、菜单功能说明】
【表格】
- 新建空白表格：清空当前表并恢复默认状态。

【编辑】
- 清空选区：删除选中单元格内容。
- 剪切 / 复制 / 粘贴：支持多单元格区域。
- 合并单元格 / 取消合并单元格：对当前选区生效。
- 增大单元格 / 缩小单元格 / 恢复默认单元格大小：调整行高列宽。
- 设置字体颜色 / 清除字体颜色：设置文本显示颜色。
- 设置填充颜色 / 清除填充颜色：设置单元格背景色。
- 排序 A->Z / 排序 Z->A：按选区首列进行排序。
- 重新计算(F9)：强制重算当前表内公式。

【公式】
- 插入 SUM / AVG / MAX / MIN / SIN / COS / COUNT / SQRT / ABS / ROUND / POW：
  会把函数模板写入当前编辑位置，便于继续补参数。

【图表】
- 折线图(Python)：适合趋势数据。
- 柱状图(Python)：适合分类对比。
- 饼图(Python)：适合占比展示（通常一组标签 + 一组数值）。
  使用步骤：先在表格中选中数据区域，再点击图表菜单项。

【文件】
- 导入测试用例(.in)：读取首行 rows cols 的文本数据。
- 打开 CSV / 打开 DAT：加载对应格式文件。
- 比较存储效率(CSV vs DAT)：手动选择 CSV 与 DAT 文件对，比较体积占比。
- 保存 / 另存为：按当前扩展名保存为 CSV 或 DAT。
- 退出：关闭程序。

【帮助】
- 功能与函数说明(F1)：打开当前这份说明。
- 关于：显示项目简介。

【三、公式函数用法】
通用说明：
- 参数可写数字、单元格（如 A1）、区域（如 A1:B10）。
- 区域函数里遇到文本或错误会返回错误。

1. SUM(x1, x2, ...)
   作用：求和，支持多个参数和区域。
   示例：=SUM(A1:A5, C1, 10)

2. AVG(x1, x2, ...)
   作用：平均值。
   示例：=AVG(B1:B10)

3. MIN(x1, x2, ...)
   作用：最小值。
   示例：=MIN(A1:A10)

4. MAX(x1, x2, ...)
   作用：最大值。
   示例：=MAX(A1:A10)

5. COUNT(x)
   作用：统计数字个数。
   注意：当前实现仅接收 1 个参数（可以是单值或区域）。
   示例：=COUNT(A1:A20)

6. SIN(x)
   作用：正弦（弧度制）。
   示例：=SIN(3.1415926/2)

7. COS(x)
   作用：余弦（弧度制）。
   示例：=COS(0)

8. SQRT(x)
   作用：平方根，x<0 会报错。
   示例：=SQRT(9)

9. ABS(x)
   作用：绝对值。
   示例：=ABS(-12.5)

10. ROUND(x, n)
    作用：四舍五入。
    用法：=ROUND(x) 或 =ROUND(x, n)
    其中 n 为保留小数位数。
    示例：=ROUND(3.14159, 2)

11. POW(base, exp)
    作用：幂运算，等价于 base^exp。
    示例：=POW(2, 10)

【四、常见问题】
1. 公式显示 0 或报错：
   检查是否存在除零、无效参数、循环引用。
2. 图表无法生成：
   检查本机 Python 与 matplotlib 是否可用，并确认选区包含有效数据。
3. 中文显示异常：
   在 Python 环境安装常见中文字体并保持系统字体可用。)");
}

QString IndexReference(const QModelIndex& index) {
    if (!index.isValid()) return {};
    return QString::fromStdString(emw::Address{index.row(), index.column()}.to_string());
}

QString RangeReference(const QModelIndex& top_left, const QModelIndex& bottom_right) {
    if (!top_left.isValid() || !bottom_right.isValid()) return {};
    const QString start = IndexReference(top_left);
    const QString end = IndexReference(bottom_right);
    return start == end ? start : QString("%1:%2").arg(start, end);
}

QString RangeReference(const QItemSelectionRange& range) {
    return RangeReference(range.topLeft(), range.bottomRight());
}

struct SelectionBounds {
    int min_row = 0;
    int max_row = 0;
    int min_col = 0;
    int max_col = 0;
};

bool TryGetSelectionBounds(const QModelIndexList& indexes, SelectionBounds* bounds) {
    if (indexes.isEmpty() || !bounds) return false;

    SelectionBounds out;
    out.min_row = indexes.front().row();
    out.max_row = indexes.front().row();
    out.min_col = indexes.front().column();
    out.max_col = indexes.front().column();

    for (const QModelIndex& index : indexes) {
        out.min_row = min(out.min_row, index.row());
        out.max_row = max(out.max_row, index.row());
        out.min_col = min(out.min_col, index.column());
        out.max_col = max(out.max_col, index.column());
    }

    *bounds = out;
    return true;
}

QString FormatNumber(double value) {
    return QString::number(value, 'g', 12);
}

bool TryParsePlainNumber(const QString& text, double* value) {
    if (!value) return false;
    bool ok = false;
    const double parsed = text.trimmed().toDouble(&ok);
    if (!ok) return false;
    *value = parsed;
    return true;
}

QString FormatAutoFillNumber(double value) {
    const double rounded = round(value);
    if (abs(value - rounded) < 1e-9) {
        return QString::number(static_cast<qlonglong>(rounded));
    }
    return QString::number(value, 'g', 15);
}

int PositiveModulo(int value, int base) {
    if (base <= 0) return 0;
    const int result = value % base;
    return result < 0 ? result + base : result;
}

bool RangesIntersect(const QItemSelectionRange& lhs, const QItemSelectionRange& rhs) {
    if (!lhs.isValid() || !rhs.isValid()) return false;
    if (lhs.bottom() < rhs.top() || rhs.bottom() < lhs.top()) return false;
    if (lhs.right() < rhs.left() || rhs.right() < lhs.left()) return false;
    return true;
}

QString EscapeCsvCell(const QString& cell) {
    QString escaped = cell;
    escaped.replace('"', "\"\"");
    if (escaped.contains(',') || escaped.contains('"') || escaped.contains('\n') || escaped.contains('\r')) {
        return QString("\"%1\"").arg(escaped);
    }
    return escaped;
}

bool ResolvePythonExecutable(QString* program, QStringList* prefix_args) {
    if (!program || !prefix_args) return false;
    prefix_args->clear();

    const QString python = QStandardPaths::findExecutable(QStringLiteral("python"));
    if (!python.isEmpty()) {
        *program = python;
        return true;
    }

    const QString python3 = QStandardPaths::findExecutable(QStringLiteral("python3"));
    if (!python3.isEmpty()) {
        *program = python3;
        return true;
    }

    const QString py = QStandardPaths::findExecutable(QStringLiteral("py"));
    if (!py.isEmpty()) {
        *program = py;
        prefix_args->push_back(QStringLiteral("-3"));
        return true;
    }

    return false;
}

void AnimateShadow(QGraphicsDropShadowEffect* shadow, qreal blur, const QPointF& offset, QObject* owner) {
    if (!shadow || !owner) return;

    auto* blur_anim = new QPropertyAnimation(shadow, "blurRadius", owner);
    blur_anim->setDuration(240);
    blur_anim->setStartValue(shadow->blurRadius());
    blur_anim->setEndValue(blur);
    blur_anim->setEasingCurve(QEasingCurve::OutCubic);
    blur_anim->start(QAbstractAnimation::DeleteWhenStopped);

    auto* offset_anim = new QPropertyAnimation(shadow, "offset", owner);
    offset_anim->setDuration(240);
    offset_anim->setStartValue(shadow->offset());
    offset_anim->setEndValue(offset);
    offset_anim->setEasingCurve(QEasingCurve::OutCubic);
    offset_anim->start(QAbstractAnimation::DeleteWhenStopped);
}

uint32_t EncodeColorRgba(const QColor& color) {
    return (static_cast<uint32_t>(color.red()) << 24) |
           (static_cast<uint32_t>(color.green()) << 16) |
           (static_cast<uint32_t>(color.blue()) << 8) |
           static_cast<uint32_t>(color.alpha());
}

QColor DecodeColorRgba(uint32_t rgba) {
    return QColor(
        static_cast<int>((rgba >> 24) & 0xFF),
        static_cast<int>((rgba >> 16) & 0xFF),
        static_cast<int>((rgba >> 8) & 0xFF),
        static_cast<int>(rgba & 0xFF)
    );
}

emw::DatCellStyle ToDatStyle(const SpreadsheetModel::CellStyle& style) {
    emw::DatCellStyle out;
    out.has_foreground = style.has_foreground;
    out.foreground_rgba = style.has_foreground ? EncodeColorRgba(style.foreground) : 0;
    out.has_background = style.has_background;
    out.background_rgba = style.has_background ? EncodeColorRgba(style.background) : 0;
    return out;
}

SpreadsheetModel::CellStyle ToModelStyle(const emw::DatCellStyle& style) {
    SpreadsheetModel::CellStyle out;
    out.has_foreground = style.has_foreground;
    out.foreground = style.has_foreground ? DecodeColorRgba(style.foreground_rgba) : QColor();
    out.has_background = style.has_background;
    out.background = style.has_background ? DecodeColorRgba(style.background_rgba) : QColor();
    return out;
}

emw::DatDocument BuildDocumentFromGrid(const emw::SpreadsheetGrid& grid, int rows, int cols) {
    emw::DatDocument document;
    document.rows = max(1, rows);
    document.cols = max(1, cols);
    grid.ForEachCell([&](const emw::Address& addr, const emw::Cell& cell) {
        if (cell.raw.empty()) return;
        emw::DatCellRecord record;
        record.addr = addr;
        record.raw = cell.raw;
        document.cells.push_back(::move(record));
    });
    return document;
}

std::shared_ptr<emw::SpreadsheetGrid> BuildGridFromDocument(const emw::DatDocument& document) {
    auto grid = std::make_shared<emw::SpreadsheetGrid>();
    for (const emw::DatCellRecord& record : document.cells) {
        if (!record.addr.is_valid()) continue;
        grid->SetCell(record.addr, record.raw, nullptr);
    }
    return grid;
}

struct SortCellKey {
    bool is_empty = true;
    bool is_number = false;
    double number = 0.0;
    QString text;
};

SortCellKey BuildSortCellKey(const QString& text) {
    SortCellKey key;
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return key;
    }

    key.is_empty = false;
    bool ok = false;
    const double parsed = trimmed.toDouble(&ok);
    if (ok) {
        key.is_number = true;
        key.number = parsed;
        return key;
    }

    key.text = trimmed;
    return key;
}

int SortCategory(const SortCellKey& key, bool ascending) {
    if (key.is_empty) return 2;
    if (ascending) {
        return key.is_number ? 0 : 1;
    }
    return key.is_number ? 1 : 0;
}

int CompareSortKey(const SortCellKey& lhs, const SortCellKey& rhs, bool ascending, QCollator* collator) {
    const int lhs_category = SortCategory(lhs, ascending);
    const int rhs_category = SortCategory(rhs, ascending);
    if (lhs_category != rhs_category) return lhs_category < rhs_category ? -1 : 1;

    if (lhs.is_empty && rhs.is_empty) return 0;
    if (lhs.is_number && rhs.is_number) {
        if (lhs.number == rhs.number) return 0;
        if (ascending) return lhs.number < rhs.number ? -1 : 1;
        return lhs.number > rhs.number ? -1 : 1;
    }

    const int cmp = collator ? collator->compare(lhs.text, rhs.text) : QString::compare(lhs.text, rhs.text, Qt::CaseInsensitive);
    if (cmp == 0) return 0;
    if (ascending) return cmp < 0 ? -1 : 1;
    return cmp > 0 ? -1 : 1;
}

QColor PickBasicColor(QWidget* parent, const QColor& initial, const QString& title) {
    const QVector<QColor> colors = {
        QColor("#000000"), QColor("#404040"), QColor("#808080"), QColor("#FFFFFF"),
        QColor("#FF0000"), QColor("#00B050"), QColor("#0070C0"), QColor("#FFC000"),
        QColor("#7030A0"), QColor("#00B0F0"), QColor("#C00000"), QColor("#00B050")
    };

    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setModal(true);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    auto* grid = new QGridLayout();
    grid->setSpacing(8);

    QColor selected = initial;
    const int cols = 4;
    for (int i = 0; i < colors.size(); ++i) {
        const QColor color = colors[i];
        auto* button = new QPushButton(&dialog);
        button->setFixedSize(40, 32);
        button->setStyleSheet(QString(
            "QPushButton { background: %1; border: 2px solid %2; border-radius: 6px; }"
        ).arg(color.name(), color == initial ? "#1f7a45" : "#d7e3d8"));
        grid->addWidget(button, i / cols, i % cols);
        QObject::connect(button, &QPushButton::clicked, &dialog, [&, color]() {
            selected = color;
            dialog.accept();
        });
    }

    layout->addLayout(grid);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() == QDialog::Accepted) {
        return selected;
    }
    return QColor();
}

QString TrimFixedTrailingZeros(QString text) {
    if (text.contains('.')) {
        while (text.endsWith('0')) text.chop(1);
        if (text.endsWith('.')) text.chop(1);
    }
    if (text == "-0") text = "0";
    return text;
}

QString BestFixedText(
    double value,
    const QFontMetrics& fm,
    int width,
    int max_precision
) {
    for (int precision = max_precision; precision >= 0; --precision) {
        const QString text = TrimFixedTrailingZeros(QString::number(value, 'f', precision));
        if (fm.horizontalAdvance(text) <= width) {
            return text;
        }
    }
    return {};
}

QString BestScientificText(
    double value,
    const QFontMetrics& fm,
    int width,
    int max_precision
) {
    for (int precision = max_precision; precision >= 0; --precision) {
        // Preserve full exponent suffix (for example: e-05) to avoid ambiguity.
        const QString text = QString::number(value, 'e', precision);
        if (fm.horizontalAdvance(text) <= width) {
            return text;
        }
    }
    return {};
}

bool ShouldPreferScientific(double value) {
    const double abs_value = fabs(value);
    if (abs_value == 0.0) return false;
    return abs_value >= 1e12 || abs_value < 1e-4;
}

QString FormatNumberToWidth(double value, const QFontMetrics& fm, int width) {
    if (width <= 0) return QStringLiteral("#");

    const int max_precision = 15;
    const bool prefer_scientific = ShouldPreferScientific(value);

    if (prefer_scientific) {
        const QString scientific_text = BestScientificText(value, fm, width, max_precision);
        return scientific_text.isEmpty() ? QStringLiteral("#") : scientific_text;
    }

    const QString fixed_text = BestFixedText(value, fm, width, max_precision);
    if (!fixed_text.isEmpty()) return fixed_text;

    const QString scientific_text = BestScientificText(value, fm, width, max_precision);
    if (!scientific_text.isEmpty()) return scientific_text;

    return QStringLiteral("#");
}

class SpreadsheetNumberDelegate : public QStyledItemDelegate {
public:
    explicit SpreadsheetNumberDelegate(QTableView* view)
        : QStyledItemDelegate(view), view_(view) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);

        const QVariant type_var = index.data(Qt::UserRole);
        if (!type_var.isValid()) {
            QStyledItemDelegate::paint(painter, opt, index);
            return;
        }

        const int type = type_var.toInt();
        if (type != static_cast<int>(emw::ValueType::Number)) {
            QStyledItemDelegate::paint(painter, opt, index);
            return;
        }

        const double number = index.data(Qt::UserRole + 1).toDouble();
        const QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();

        QStyleOptionViewItem base_opt(opt);
        base_opt.text.clear();
        base_opt.displayAlignment = Qt::AlignLeft | Qt::AlignVCenter;
        base_opt.textElideMode = Qt::ElideNone;
        style->drawControl(QStyle::CE_ItemViewItem, &base_opt, painter, base_opt.widget);

        const QRect text_rect = style->subElementRect(QStyle::SE_ItemViewItemText, &base_opt, base_opt.widget);
        const QString text = FormatNumberToWidth(number, base_opt.fontMetrics, max(0, text_rect.width() - 1));

        painter->save();
        painter->setClipRect(text_rect);
        const QPalette::ColorRole text_role =
            (base_opt.state & QStyle::State_Selected) ? QPalette::HighlightedText : QPalette::Text;
        painter->setPen(base_opt.palette.color(text_role));
        painter->setFont(base_opt.font);
        painter->drawText(text_rect, Qt::AlignLeft | Qt::AlignVCenter, text);
        painter->restore();
    }

private:
    QTableView* view_ = nullptr;
};

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    model_ = new SpreadsheetModel(this);
    table_ = new SpreadsheetView(this);
    formula_bar_ = new QLineEdit(this);
    name_box_label_ = new QLabel(this);
    selection_summary_label_ = new QLabel(this);
    shortcut_hint_label_ = new QLabel(this);
    position_label_ = new QLabel(this);
    value_type_label_ = new QLabel(this);
    open_watcher_ = new QFutureWatcher<OpenDocumentResult>(this);

    connect(open_watcher_, &QFutureWatcher<OpenDocumentResult>::finished, this, [this]() {
        OnOpenDocumentFinished();
    });

    SetupWindow();
    SetupMenuBar();
    SetupToolbar();
    SetupTable();
    SetupStatusBar();
    SetupConnections();
    SetupCardEffects();
    PlayIntroAnimations();
    UpdateSelectionInfo();
}

void MainWindow::SetupWindow() {
    setWindowTitle(QString::fromUtf8("Excel Mowan"));
    resize(1380, 900);
    setMinimumSize(1080, 720);

    auto* root = new QWidget(this);
    auto* layout = new QVBoxLayout(root);
    layout->setContentsMargins(18, 14, 18, 18);
    layout->setSpacing(12);

    auto* formula_card = new QFrame(root);
    formula_card->setObjectName("formulaCard");
    formula_card_ = formula_card;
    auto* formula_layout = new QHBoxLayout(formula_card);
    formula_layout->setContentsMargins(16, 14, 16, 14);
    formula_layout->setSpacing(12);

    name_box_label_->setObjectName("nameBoxLabel");
    name_box_label_->setAlignment(Qt::AlignCenter);
    name_box_label_->setMinimumWidth(110);

    auto* fx_label = new QLabel(QStringLiteral("fx"), formula_card);
    fx_label->setObjectName("fxLabel");
    fx_label->setFixedWidth(38);
    fx_label->setAlignment(Qt::AlignCenter);

    formula_bar_->setPlaceholderText(DefaultFormulaPlaceholder());
    formula_bar_->setClearButtonEnabled(true);
    formula_bar_->setMinimumHeight(44);

    formula_layout->addWidget(name_box_label_);
    formula_layout->addWidget(fx_label);
    formula_layout->addWidget(formula_bar_, 1);

    auto* table_card = new QFrame(root);
    table_card->setObjectName("tableCard");
    table_card_ = table_card;
    auto* table_layout = new QVBoxLayout(table_card);
    table_layout->setContentsMargins(16, 16, 16, 16);
    table_layout->setSpacing(12);

    auto* info_row = new QHBoxLayout();
    info_row->setSpacing(10);

    selection_summary_label_->setObjectName("infoPill");
    selection_summary_label_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    shortcut_hint_label_->setObjectName("hintPill");
    shortcut_hint_label_->setAlignment(Qt::AlignCenter);

    info_row->addWidget(selection_summary_label_, 1);
    info_row->addWidget(shortcut_hint_label_);

    table_layout->addLayout(info_row);
    table_layout->addWidget(table_, 1);

    layout->addWidget(formula_card);
    layout->addWidget(table_card, 1);
    setCentralWidget(root);
    formula_card_->hide();

    setStyleSheet(
        "QMainWindow { background: #eef3ec; }"
        "QMenuBar { background: #f6faf5; color: #1f3425; border-bottom: 1px solid #d7e3d8; padding: 4px 8px; }"
        "QMenuBar::item { background: transparent; padding: 8px 12px; border-radius: 8px; }"
        "QMenuBar::item:selected { background: #dfeee0; }"
        "QMenu { background: white; border: 1px solid #d7e3d8; padding: 6px; }"
        "QMenu::item { padding: 8px 22px; border-radius: 8px; }"
        "QMenu::item:selected { background: #dfeee0; }"
        "QFrame#formulaCard, QFrame#tableCard { background: #fcfefd; border: 1px solid #d7e3d8; border-radius: 20px; }"
        "QLabel { color: #1f3425; }"
        "QLabel#nameBoxLabel { background: #e8f3e8; color: #205629; border: 1px solid #c7ddc8; border-radius: 12px; padding: 10px 14px; font-weight: 700; }"
        "QLabel#fxLabel { background: #217346; color: white; border-radius: 10px; font-weight: 700; }"
        "QLabel#infoPill, QLabel#hintPill { background: #f5faf4; color: #35533b; border: 1px solid #d9e8d9; border-radius: 999px; padding: 8px 14px; }"
        "QLineEdit, QComboBox, QPlainTextEdit { background: white; selection-background-color: #1f7a45; selection-color: white; }"
        "QLineEdit, QComboBox { border: 1px solid #cbdcca; border-radius: 12px; padding: 9px 14px; }"
        "QLineEdit:focus, QComboBox:focus, QPlainTextEdit:focus { border: 1px solid #217346; }"
        "QPlainTextEdit { border: 1px solid #d3dfd4; border-radius: 14px; padding: 10px 12px; }"
        "QTableView { background: white; border: none; gridline-color: #e4ece3; alternate-background-color: #f7fbf6; selection-background-color: #d8ead9; selection-color: #17311e; outline: none; }"
        "QTableView::item { padding: 4px 8px; }"
        "QTableView::item:selected { background: #d8ead9; color: #17311e; }"
        "QTableView::item:selected:active { background: #cbe4cd; }"
        "QHeaderView::section { background: #edf5ec; color: #2a4730; border: none; border-right: 1px solid #d7e3d8; border-bottom: 1px solid #d7e3d8; padding: 8px; font-weight: 700; }"
        "QCornerButton::section { background: #edf5ec; border: none; border-right: 1px solid #d7e3d8; border-bottom: 1px solid #d7e3d8; }"
        "QScrollBar:vertical { background: transparent; width: 12px; margin: 8px 2px 8px 2px; }"
        "QScrollBar::handle:vertical { background: #bfd3c0; border-radius: 6px; min-height: 28px; }"
        "QScrollBar:horizontal { background: transparent; height: 12px; margin: 2px 8px 2px 8px; }"
        "QScrollBar::handle:horizontal { background: #bfd3c0; border-radius: 6px; min-width: 28px; }"
        "QScrollBar::add-line, QScrollBar::sub-line, QScrollBar::add-page, QScrollBar::sub-page { background: transparent; border: none; }"
        "QStatusBar { background: #fcfefd; border-top: 1px solid #d7e3d8; color: #35533b; }"
    );
}

void MainWindow::SetupMenuBar() {
    auto* table_menu = menuBar()->addMenu(QStringLiteral("\u8868\u683c(&T)"));
    auto* edit_menu = menuBar()->addMenu(QStringLiteral("\u7f16\u8f91(&E)"));
    auto* formula_menu = menuBar()->addMenu(QStringLiteral("\u516c\u5f0f(&M)"));
    auto* chart_menu = menuBar()->addMenu(QStringLiteral("\u56fe\u8868(&A)"));
    auto* file_menu = menuBar()->addMenu(QStringLiteral("\u6587\u4ef6(&F)"));
    auto* help_menu = menuBar()->addMenu(QStringLiteral("\u5e2e\u52a9(&H)"));

    auto* new_sheet_action = table_menu->addAction(QStringLiteral("\u65b0\u5efa\u7a7a\u767d\u8868\u683c"));
    new_sheet_action->setShortcut(QKeySequence::New);
    connect(new_sheet_action, &QAction::triggered, this, &MainWindow::ResetCurrentSheet);

    auto* clear_selection_action = edit_menu->addAction(QStringLiteral("\u6e05\u7a7a\u9009\u533a"));
    clear_selection_action->setShortcut(QKeySequence::Delete);
    connect(clear_selection_action, &QAction::triggered, this, &MainWindow::ClearSelectedCells);

    auto* cut_action = edit_menu->addAction(QStringLiteral("\u526a\u5207"));
    cut_action->setShortcut(QKeySequence::Cut);
    connect(cut_action, &QAction::triggered, this, [this]() { CopySelectionToClipboard(true); });

    auto* copy_action = edit_menu->addAction(QStringLiteral("\u590d\u5236"));
    copy_action->setShortcut(QKeySequence::Copy);
    connect(copy_action, &QAction::triggered, this, [this]() { CopySelectionToClipboard(false); });

    auto* paste_action = edit_menu->addAction(QStringLiteral("\u7c98\u8d34"));
    paste_action->setShortcut(QKeySequence::Paste);
    connect(paste_action, &QAction::triggered, this, &MainWindow::PasteFromClipboard);

    edit_menu->addSeparator();

    auto* merge_cells_action = edit_menu->addAction(QStringLiteral("\u5408\u5e76\u5355\u5143\u683c"));
    merge_cells_action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
    connect(merge_cells_action, &QAction::triggered, this, &MainWindow::MergeSelectedCells);

    auto* unmerge_cells_action = edit_menu->addAction(QStringLiteral("\u53d6\u6d88\u5408\u5e76\u5355\u5143\u683c"));
    unmerge_cells_action->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M));
    connect(unmerge_cells_action, &QAction::triggered, this, &MainWindow::UnmergeSelectedCells);

    auto* enlarge_cells_action = edit_menu->addAction(QStringLiteral("\u589e\u5927\u5355\u5143\u683c"));
    enlarge_cells_action->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Plus));
    connect(enlarge_cells_action, &QAction::triggered, this, [this]() {
        ResizeSelectedCells(kCellResizeStep, kCellResizeStep / 2);
    });

    auto* shrink_cells_action = edit_menu->addAction(QStringLiteral("\u7f29\u5c0f\u5355\u5143\u683c"));
    shrink_cells_action->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Underscore));
    connect(shrink_cells_action, &QAction::triggered, this, [this]() {
        ResizeSelectedCells(-kCellResizeStep, -(kCellResizeStep / 2));
    });

    auto* reset_cell_size_action = edit_menu->addAction(QStringLiteral("\u6062\u590d\u9ed8\u8ba4\u5355\u5143\u683c\u5927\u5c0f"));
    reset_cell_size_action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(reset_cell_size_action, &QAction::triggered, this, &MainWindow::ResetSelectedCellSizes);

    edit_menu->addSeparator();

    auto* text_color_action = edit_menu->addAction(QStringLiteral("\u8bbe\u7f6e\u5b57\u4f53\u989c\u8272"));
    connect(text_color_action, &QAction::triggered, this, &MainWindow::SetSelectedTextColor);

    auto* clear_text_color_action = edit_menu->addAction(QStringLiteral("\u6e05\u9664\u5b57\u4f53\u989c\u8272"));
    connect(clear_text_color_action, &QAction::triggered, this, &MainWindow::ClearSelectedTextColor);

    auto* fill_color_action = edit_menu->addAction(QStringLiteral("\u8bbe\u7f6e\u586b\u5145\u989c\u8272"));
    connect(fill_color_action, &QAction::triggered, this, &MainWindow::SetSelectedFillColor);

    auto* clear_fill_color_action = edit_menu->addAction(QStringLiteral("\u6e05\u9664\u586b\u5145\u989c\u8272"));
    connect(clear_fill_color_action, &QAction::triggered, this, &MainWindow::ClearSelectedFillColor);

    edit_menu->addSeparator();

    auto* sort_asc_action = edit_menu->addAction(QStringLiteral("\u6392\u5e8f A -> Z"));
    connect(sort_asc_action, &QAction::triggered, this, [this]() { SortSelectedRange(true); });

    auto* sort_desc_action = edit_menu->addAction(QStringLiteral("\u6392\u5e8f Z -> A"));
    connect(sort_desc_action, &QAction::triggered, this, [this]() { SortSelectedRange(false); });

    edit_menu->addSeparator();

    auto* recalc_action = edit_menu->addAction(QStringLiteral("\u91cd\u65b0\u8ba1\u7b97"));
    recalc_action->setShortcut(Qt::Key_F9);
    connect(recalc_action, &QAction::triggered, this, [this]() {
        model_->recalcAll();
        statusBar()->showMessage(QStringLiteral("\u5df2\u91cd\u65b0\u8ba1\u7b97\u5168\u90e8\u516c\u5f0f"), 1500);
        UpdateSelectionInfo(!IsFormulaEditingMode());
    });

    formula_menu->addAction(QStringLiteral("\u63d2\u5165 SUM"), this, [this]() { InsertFunctionTemplate("SUM"); });
    formula_menu->addAction(QStringLiteral("\u63d2\u5165 AVG"), this, [this]() { InsertFunctionTemplate("AVG"); });
    formula_menu->addAction(QStringLiteral("\u63d2\u5165 MAX"), this, [this]() { InsertFunctionTemplate("MAX"); });
    formula_menu->addAction(QStringLiteral("\u63d2\u5165 MIN"), this, [this]() { InsertFunctionTemplate("MIN"); });
    formula_menu->addAction(QStringLiteral("\u63d2\u5165 SIN"), this, [this]() { InsertFunctionTemplate("SIN"); });
    formula_menu->addAction(QStringLiteral("\u63d2\u5165 COS"), this, [this]() { InsertFunctionTemplate("COS"); });
    formula_menu->addAction(QStringLiteral("\u63d2\u5165 COUNT"), this, [this]() { InsertFunctionTemplate("COUNT"); });
    formula_menu->addAction(QStringLiteral("\u63d2\u5165 SQRT"), this, [this]() { InsertFunctionTemplate("SQRT"); });
    formula_menu->addAction(QStringLiteral("\u63d2\u5165 ABS"), this, [this]() { InsertFunctionTemplate("ABS"); });
    formula_menu->addAction(QStringLiteral("\u63d2\u5165 ROUND"), this, [this]() { InsertFunctionTemplate("ROUND"); });
    formula_menu->addAction(QStringLiteral("\u63d2\u5165 POW"), this, [this]() { InsertFunctionTemplate("POW"); });

    auto* line_chart_action = chart_menu->addAction(QStringLiteral("\u6298\u7ebf\u56fe(Python)"));
    connect(line_chart_action, &QAction::triggered, this, [this]() { PlotSelectionWithPython("line"); });
    auto* bar_chart_action = chart_menu->addAction(QStringLiteral("\u67f1\u72b6\u56fe(Python)"));
    connect(bar_chart_action, &QAction::triggered, this, [this]() { PlotSelectionWithPython("bar"); });
    auto* pie_chart_action = chart_menu->addAction(QStringLiteral("\u997c\u56fe(Python)"));
    connect(pie_chart_action, &QAction::triggered, this, [this]() { PlotSelectionWithPython("pie"); });

    auto* import_in_action = file_menu->addAction(QStringLiteral("\u5bfc\u5165\u6d4b\u8bd5\u7528\u4f8b(.in)..."));
    import_in_action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));
    connect(import_in_action, &QAction::triggered, this, &MainWindow::ImportInFile);

    auto* open_csv_action = file_menu->addAction(QStringLiteral("\u6253\u5f00 CSV..."));
    connect(open_csv_action, &QAction::triggered, this, &MainWindow::OpenCsvFile);

    auto* open_dat_action = file_menu->addAction(QStringLiteral("\u6253\u5f00 DAT..."));
    open_dat_action->setShortcut(QKeySequence::Open);
    connect(open_dat_action, &QAction::triggered, this, &MainWindow::OpenDatFile);

    file_menu->addSeparator();
    auto* compare_storage_action = file_menu->addAction(QStringLiteral("\u6bd4\u8f83\u5b58\u50a8\u6548\u7387(CSV vs DAT)..."));
    connect(compare_storage_action, &QAction::triggered, this, &MainWindow::CompareStorageEfficiencyFiles);

    file_menu->addSeparator();
    auto* save_action = file_menu->addAction(QStringLiteral("\u4fdd\u5b58"));
    save_action->setShortcut(QKeySequence::Save);
    connect(save_action, &QAction::triggered, this, &MainWindow::SaveFile);

    auto* save_as_action = file_menu->addAction(QStringLiteral("\u53e6\u5b58\u4e3a..."));
    save_as_action->setShortcut(QKeySequence::SaveAs);
    connect(save_as_action, &QAction::triggered, this, &MainWindow::SaveFileAs);

    auto* exit_action = file_menu->addAction(QStringLiteral("\u9000\u51fa"));
    exit_action->setShortcut(QKeySequence::Quit);
    connect(exit_action, &QAction::triggered, this, &QWidget::close);

    auto* guide_action = help_menu->addAction(QStringLiteral("\u529f\u80fd\u4e0e\u51fd\u6570\u8bf4\u660e"));
    guide_action->setShortcut(QKeySequence(Qt::Key_F1));
    connect(guide_action, &QAction::triggered, this, &MainWindow::ShowHelpGuide);

    help_menu->addSeparator();

    auto* about_action = help_menu->addAction(QStringLiteral("\u5173\u4e8e"));
    connect(about_action, &QAction::triggered, this, [this]() {
        QMessageBox::information(
            this,
            QStringLiteral("\u5173\u4e8e Excel Mowan"),
            QStringLiteral("Excel Mowan \u662f\u4e00\u4e2a\u8f7b\u91cf Qt \u8868\u683c\u5de5\u5177\uff0c\u652f\u6301\u516c\u5f0f\u4e0e Python \u56fe\u8868\u7ed8\u5236\u3002")
        );
    });
}

void MainWindow::ShowHelpGuide() {
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("功能与函数说明"));
    dialog.setMinimumSize(860, 640);

    auto* layout = new QVBoxLayout(&dialog);

    auto* editor = new QPlainTextEdit(&dialog);
    editor->setReadOnly(true);
    editor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    editor->setPlainText(BuildHelpGuideText());
    layout->addWidget(editor);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    dialog.exec();
}

void MainWindow::SetupToolbar() {
    formula_bar_->clear();

    auto* format_toolbar = addToolBar(QStringLiteral("格式"));
    format_toolbar->setObjectName(QStringLiteral("formatToolbar"));
    format_toolbar->setMovable(false);
    format_toolbar->setFloatable(false);
    format_toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);

    auto* text_color_action = format_toolbar->addAction(QStringLiteral("字体颜色"));
    text_color_action->setToolTip(QStringLiteral("设置选中单元格的字体颜色"));
    connect(text_color_action, &QAction::triggered, this, &MainWindow::SetSelectedTextColor);

    auto* clear_text_color_action = format_toolbar->addAction(QStringLiteral("清除字体颜色"));
    clear_text_color_action->setToolTip(QStringLiteral("清除选中单元格的字体颜色"));
    connect(clear_text_color_action, &QAction::triggered, this, &MainWindow::ClearSelectedTextColor);

    auto* fill_color_action = format_toolbar->addAction(QStringLiteral("填充颜色"));
    fill_color_action->setToolTip(QStringLiteral("设置选中单元格填充颜色"));
    connect(fill_color_action, &QAction::triggered, this, &MainWindow::SetSelectedFillColor);

    auto* clear_fill_color_action = format_toolbar->addAction(QStringLiteral("清除填充颜色"));
    clear_fill_color_action->setToolTip(QStringLiteral("清除选中单元格填充颜色"));
    connect(clear_fill_color_action, &QAction::triggered, this, &MainWindow::ClearSelectedFillColor);

    auto* sort_asc_action = format_toolbar->addAction(QStringLiteral("升序 A->Z"));
    sort_asc_action->setToolTip(QStringLiteral("按选区首列升序排序"));
    connect(sort_asc_action, &QAction::triggered, this, [this]() { SortSelectedRange(true); });

    auto* sort_desc_action = format_toolbar->addAction(QStringLiteral("降序 Z->A"));
    sort_desc_action->setToolTip(QStringLiteral("按选区首列降序排序"));
    connect(sort_desc_action, &QAction::triggered, this, [this]() { SortSelectedRange(false); });
}
void MainWindow::SetupTable() {
    table_->setModel(model_);
    table_->setItemDelegate(new SpreadsheetNumberDelegate(table_));
    table_->setAlternatingRowColors(true);
    table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_->setSelectionBehavior(QAbstractItemView::SelectItems);
    table_->setDragDropMode(QAbstractItemView::NoDragDrop);
    table_->setDragEnabled(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setShowGrid(true);
    table_->setWordWrap(false);
    table_->setTextElideMode(Qt::ElideNone);
    table_->setCornerButtonEnabled(true);
    table_->setSortingEnabled(false);
    table_->setTabKeyNavigation(true);
    table_->setMouseTracking(true);
    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    table_->setAutoScroll(true);
    table_->setAutoScrollMargin(24);
    table_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    table_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    // Excel-like: double-click row/column separators to auto-fit size.
    if (auto* h = table_->horizontalHeader()) {
        h->setSectionResizeMode(QHeaderView::Interactive);
        connect(h, &QHeaderView::sectionDoubleClicked, this, [this](int section) {
            table_->resizeColumnToContents(section);
        });
    }
    if (auto* v = table_->verticalHeader()) {
        v->setSectionResizeMode(QHeaderView::Interactive);
        connect(v, &QHeaderView::sectionDoubleClicked, this, [this](int section) {
            table_->resizeRowToContents(section);
        });
    }
    table_->setFrameShape(QFrame::NoFrame);

    table_->verticalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table_->verticalHeader()->setDefaultSectionSize(kDefaultRowHeight);
    table_->verticalHeader()->setMinimumSectionSize(22);
    table_->verticalHeader()->setMinimumWidth(56);
    table_->verticalHeader()->setHighlightSections(false);
    table_->verticalHeader()->setDefaultAlignment(Qt::AlignCenter);

    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    table_->horizontalHeader()->setDefaultSectionSize(kDefaultColumnWidth);
    table_->horizontalHeader()->setMinimumSectionSize(90);
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setHighlightSections(false);
    table_->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    table_->horizontalHeader()->setFixedHeight(40);

    table_->setCurrentIndex(model_->index(0, 0));
    table_->setFocus();
}

void MainWindow::SetupStatusBar() {
    position_label_->setText(QStringLiteral("\u9009\u533a\uff1aA1"));
    value_type_label_->setText(QStringLiteral("\u7c7b\u578b\uff1a\u7a7a\u503c"));
    statusBar()->addWidget(position_label_);
    statusBar()->addPermanentWidget(value_type_label_);
    statusBar()->showMessage(QStringLiteral("\u8868\u683c\u5df2\u5c31\u7eea"), 2000);
}

void MainWindow::SetupConnections() {
    formula_bar_->installEventFilter(this);

    table_->SetInlineEditStartedCallback([this](const QModelIndex& index, const QString& text) {
        editing_index_ = index;
        SyncFormulaBarFromInlineEditor();

        if (text.startsWith('=')) {
            formula_editor_source_ = FormulaEditorSource::InlineCell;
            StartFormulaEditingMode();
        } else if (formula_editor_source_ == FormulaEditorSource::InlineCell) {
            EndFormulaEditingMode();
        }
        UpdateSelectionInfo(false);
    });

    table_->SetInlineEditTextChangedCallback([this](const QModelIndex& index, const QString& text) {
        if (internal_inline_sync_) return;

        editing_index_ = index;
        formula_editor_source_ = FormulaEditorSource::InlineCell;
        SyncFormulaBarFromInlineEditor();

        if (text.startsWith('=')) {
            ClearFormulaReferenceTracking();
            StartFormulaEditingMode();
        } else if (formula_editor_source_ == FormulaEditorSource::InlineCell) {
            EndFormulaEditingMode();
        }
        UpdateSelectionInfo(false);
    });

    table_->SetInlineEditContextChangedCallback([this]() {
        if (formula_editor_source_ == FormulaEditorSource::InlineCell && table_->HasInlineEditor()) {
            ClearFormulaReferenceTracking();
        }
    });

    table_->SetInlineEditFinishedCallback([this](const QModelIndex&, bool) {
        formula_editor_source_ = FormulaEditorSource::None;
        EndFormulaEditingMode();
        UpdateSelectionInfo();
    });

    table_->SetFillHandleDraggedCallback([this](const QItemSelectionRange& source, const QItemSelectionRange& target) {
        ApplyAutoFill(source, target);
    });


    connect(formula_bar_, &QLineEdit::returnPressed, this, [this]() { ApplyFormulaBarToCurrentCell(); });

    connect(formula_bar_, &QLineEdit::textEdited, this, [this](const QString& text) {
        if (table_->HasInlineEditor()) {
            editing_index_ = table_->InlineEditIndex();
            formula_editor_source_ = FormulaEditorSource::FormulaBar;
            SyncInlineEditorFromFormulaBar();
        }

        if (text.startsWith('=')) {
            formula_editor_source_ = FormulaEditorSource::FormulaBar;
            StartFormulaEditingMode();
            ClearFormulaReferenceTracking();
        } else if (!table_->IsInlineFormulaEditing()) {
            EndFormulaEditingMode();
        }
        UpdateSelectionInfo(false);
    });

    connect(formula_bar_, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (internal_formula_update_) return;
        if (formula_editing_ && !text.startsWith('=') && !table_->IsInlineFormulaEditing()) {
            EndFormulaEditingMode();
        }
    });

    connect(formula_bar_, &QLineEdit::cursorPositionChanged, this, [this](int, int) {
        if (internal_formula_update_) return;
        if (formula_bar_->hasFocus()) {
            formula_editor_source_ = FormulaEditorSource::FormulaBar;
            ClearFormulaReferenceTracking();
            if (table_->HasInlineEditor()) {
                SyncInlineEditorFromFormulaBar();
            }
        }
    });

    connect(formula_bar_, &QLineEdit::selectionChanged, this, [this]() {
        if (internal_formula_update_) return;
        if (formula_bar_->hasFocus()) {
            formula_editor_source_ = FormulaEditorSource::FormulaBar;
            ClearFormulaReferenceTracking();
            if (table_->HasInlineEditor()) {
                SyncInlineEditorFromFormulaBar();
            }
        }
    });

    auto* selection_model = table_->selectionModel();
    connect(selection_model, &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex&, const QModelIndex&) {
                UpdateSelectionInfo(!IsFormulaEditingMode() && !table_->HasInlineEditor());
            });

    connect(selection_model, &QItemSelectionModel::selectionChanged, this,
            [this](const QItemSelection&, const QItemSelection&) {
                if (IsFormulaEditingMode()) {
                    if (!ShouldCaptureSelectionAsFormulaReference()) {
                        CommitActiveFormulaEditToCell();
                        UpdateSelectionInfo();
                        return;
                    }
                    UpdateFormulaReferenceFromSelection();
                    UpdateSelectionInfo(false);
                    return;
                }
                UpdateSelectionInfo(!table_->HasInlineEditor());
            });

    connect(table_, &QWidget::customContextMenuRequested, this, &MainWindow::ShowTableContextMenu);
}

void MainWindow::SetupCardEffects() {
    formula_shadow_ = new QGraphicsDropShadowEffect(this);
    formula_shadow_->setBlurRadius(26);
    formula_shadow_->setOffset(0, 10);
    formula_shadow_->setColor(QColor(33, 64, 40, 28));
    formula_card_->setGraphicsEffect(formula_shadow_);

    table_shadow_ = new QGraphicsDropShadowEffect(this);
    table_shadow_->setBlurRadius(32);
    table_shadow_->setOffset(0, 14);
    table_shadow_->setColor(QColor(33, 64, 40, 30));
    table_card_->setGraphicsEffect(table_shadow_);
}

void MainWindow::PlayIntroAnimations() {
    QTimer::singleShot(0, this, [this]() {
        AnimateShadow(formula_shadow_, 30, QPointF(0, 12), this);
        QTimer::singleShot(80, this, [this]() { AnimateShadow(table_shadow_, 38, QPointF(0, 18), this); });
    });
}

QModelIndexList MainWindow::SelectedIndexes() const {
    QModelIndexList indexes;
    if (!table_->selectionModel()) return indexes;

    indexes = table_->selectionModel()->selectedIndexes();
    if (indexes.isEmpty() && table_->currentIndex().isValid()) {
        indexes.push_back(table_->currentIndex());
    }

    sort(indexes.begin(), indexes.end(), [](const QModelIndex& lhs, const QModelIndex& rhs) {
        if (lhs.row() != rhs.row()) return lhs.row() < rhs.row();
        return lhs.column() < rhs.column();
    });

    indexes.erase(
        unique(indexes.begin(), indexes.end(), [](const QModelIndex& lhs, const QModelIndex& rhs) {
            return lhs.row() == rhs.row() && lhs.column() == rhs.column();
        }),
        indexes.end()
    );

    return indexes;
}

QItemSelectionRange MainWindow::CurrentSelectionRange() const {
    const QModelIndexList indexes = SelectedIndexes();
    if (indexes.isEmpty()) return {};

    SelectionBounds bounds;
    if (!TryGetSelectionBounds(indexes, &bounds)) return {};
    return QItemSelectionRange(model_->index(bounds.min_row, bounds.min_col), model_->index(bounds.max_row, bounds.max_col));
}

QString MainWindow::SelectionClipboardText() const {
    const QItemSelectionRange range = CurrentSelectionRange();
    if (!range.isValid()) return {};

    QStringList rows;
    for (int row = range.top(); row <= range.bottom(); ++row) {
        QStringList cols;
        for (int col = range.left(); col <= range.right(); ++col) {
            cols << model_->data(model_->index(row, col), Qt::EditRole).toString();
        }
        rows << cols.join('\t');
    }
    return rows.join('\n');
}

QString MainWindow::ResolvePlotScriptPath() const {
    const QString relative_path = QStringLiteral("tools/plot_selection.py");
    const QString app_dir = QCoreApplication::applicationDirPath();

    QStringList base_dirs;
    base_dirs << QDir::currentPath() << app_dir;

    QDir dir(app_dir);
    for (int i = 0; i < 4; ++i) {
        if (!dir.cdUp()) break;
        base_dirs << dir.absolutePath();
    }

    for (const QString& base_dir : base_dirs) {
        const QString candidate = QDir(base_dir).filePath(relative_path);
        if (QFileInfo::exists(candidate)) {
            return QFileInfo(candidate).absoluteFilePath();
        }
    }
    return {};
}

QString MainWindow::ExportSelectionCsvForPlot(const QItemSelectionRange& range) const {
    if (!range.isValid()) return {};

    QString temp_root = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (temp_root.isEmpty()) {
        temp_root = QDir::tempPath();
    }
    QDir output_dir(QDir(temp_root).filePath(QStringLiteral("excel_mowan_charts")));
    if (!output_dir.exists() && !output_dir.mkpath(QStringLiteral("."))) {
        return {};
    }

    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    const QString csv_path = output_dir.filePath(QStringLiteral("selection_%1.csv").arg(stamp));
    QFile file(csv_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return {};
    }

    QByteArray content;
    for (int row = range.top(); row <= range.bottom(); ++row) {
        QStringList fields;
        for (int col = range.left(); col <= range.right(); ++col) {
            const QString cell = model_->data(model_->index(row, col), Qt::DisplayRole).toString();
            fields << EscapeCsvCell(cell);
        }
        content.append(fields.join(',').toUtf8());
        content.append('\n');
    }

    const qint64 written = file.write(content);
    file.close();
    if (written != content.size()) {
        QFile::remove(csv_path);
        return {};
    }
    return csv_path;
}

void MainWindow::PlotSelectionWithPython(const QString& chart_type) {
    if (table_->HasInlineEditor()) {
        table_->CommitInlineEditor();
    }

    const QItemSelectionRange range = CurrentSelectionRange();
    if (!range.isValid()) {
        QMessageBox::information(this, QStringLiteral("\u7ed8\u56fe"), QStringLiteral("\u8bf7\u5148\u9009\u62e9\u6570\u636e\u533a\u57df\u3002"));
        return;
    }

    const QString csv_path = ExportSelectionCsvForPlot(range);
    if (csv_path.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("\u7ed8\u56fe"), QStringLiteral("\u9009\u533a\u5bfc\u51fa CSV \u5931\u8d25\u3002"));
        return;
    }

    const QString script_path = ResolvePlotScriptPath();
    if (script_path.isEmpty()) {
        QMessageBox::warning(
            this,
            QStringLiteral("\u7ed8\u56fe"),
            QStringLiteral("\u627e\u4e0d\u5230 tools/plot_selection.py\uff0c\u8bf7\u4fdd\u6301 tools \u76ee\u5f55\u5728\u9879\u76ee\u6839\u76ee\u5f55\u3002")
        );
        return;
    }

    QString program;
    QStringList prefix_args;
    if (!ResolvePythonExecutable(&program, &prefix_args)) {
        QMessageBox::warning(
            this,
            QStringLiteral("\u7ed8\u56fe"),
            QStringLiteral("\u672a\u627e\u5230 Python\uff0c\u8bf7\u786e\u8ba4 PATH \u5df2\u914d\u7f6e\uff0c\u5e76\u5b89\u88c5 matplotlib\u3002")
        );
        return;
    }

    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    const QString chart_dir = QFileInfo(csv_path).absolutePath();
    const QString output_path = QDir(chart_dir).filePath(
        QStringLiteral("chart_%1_%2.png").arg(chart_type.isEmpty() ? QStringLiteral("line") : chart_type, stamp)
    );

    QStringList args = prefix_args;
    args << script_path
         << QStringLiteral("--input") << csv_path
         << QStringLiteral("--chart") << (chart_type.isEmpty() ? QStringLiteral("line") : chart_type)
         << QStringLiteral("--output") << output_path
         << QStringLiteral("--title") << QStringLiteral("选区 %1").arg(RangeReference(range));

    QProcess process(this);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(program, args);
    if (!process.waitForStarted(5000)) {
        QMessageBox::warning(this, QStringLiteral("\u7ed8\u56fe"), QStringLiteral("Python \u8fdb\u7a0b\u542f\u52a8\u5931\u8d25\u3002"));
        return;
    }
    if (!process.waitForFinished(60000)) {
        process.kill();
        QMessageBox::warning(this, QStringLiteral("\u7ed8\u56fe"), QStringLiteral("Python \u7ed8\u56fe\u8d85\u65f6\u3002"));
        return;
    }

    const QString process_output = QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed();
    const bool ok = process.exitStatus() == QProcess::NormalExit &&
                    process.exitCode() == 0 &&
                    QFileInfo::exists(output_path);
    if (!ok) {
        const QString details = process_output.isEmpty()
                                    ? QStringLiteral("Python \u6267\u884c\u5931\u8d25\uff08\u672a\u77e5\u9519\u8bef\uff09\u3002")
                                    : process_output;
        QMessageBox::warning(
            this,
            QStringLiteral("\u7ed8\u56fe"),
            QStringLiteral("\u751f\u6210\u56fe\u8868\u5931\u8d25\u3002\n\n%1").arg(details)
        );
        return;
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(output_path));
    statusBar()->showMessage(QStringLiteral("\u56fe\u8868\u5df2\u751f\u6210\uff1a%1").arg(output_path), 4000);
}

int MainWindow::FindMergedRangeContaining(const QModelIndex& index) const {
    if (!index.isValid()) return -1;
    for (int i = 0; i < merged_ranges_.size(); ++i) {
        const QItemSelectionRange& range = merged_ranges_.at(i);
        if (!range.isValid()) continue;
        if (index.row() >= range.top() && index.row() <= range.bottom() &&
            index.column() >= range.left() && index.column() <= range.right()) {
            return i;
        }
    }
    return -1;
}

void MainWindow::UnmergeRangesIntersecting(const QItemSelectionRange& range) {
    if (!range.isValid()) return;

    for (int i = merged_ranges_.size() - 1; i >= 0; --i) {
        const QItemSelectionRange current = merged_ranges_.at(i);
        if (!RangesIntersect(current, range)) continue;
        table_->setSpan(current.top(), current.left(), 1, 1);
        merged_ranges_.removeAt(i);
    }
}

void MainWindow::CopySelectionToClipboard(bool cut) {
    if (table_->HasInlineEditor()) {
        table_->CommitInlineEditor();
    }

    const QString text = SelectionClipboardText();
    if (text.isEmpty()) return;

    QApplication::clipboard()->setText(text);
    statusBar()->showMessage(
        cut ? QStringLiteral("\u5df2\u526a\u5207\u9009\u533a") : QStringLiteral("\u5df2\u590d\u5236\u9009\u533a"),
        1500
    );

    if (cut) {
        ClearSelectedCells();
    }
}

void MainWindow::PasteFromClipboard() {
    if (table_->HasInlineEditor()) {
        table_->CommitInlineEditor();
    }

    const QString clipboard_text = QApplication::clipboard()->text();
    if (clipboard_text.isEmpty()) return;

    QModelIndex start = table_->currentIndex();
    if (!start.isValid()) {
        start = model_->index(0, 0);
    }

    QStringList rows = clipboard_text.split('\n', Qt::KeepEmptyParts);
    if (!rows.isEmpty() && rows.back().isEmpty()) {
        rows.pop_back();
    }
    if (rows.isEmpty()) return;

    int max_width = 0;
    QVector<QStringList> parsed_rows;
    for (QString row_text : rows) {
        row_text.remove('\r');
        const QStringList cols = row_text.split('\t', Qt::KeepEmptyParts);
        max_width = max(max_width, static_cast<int>(cols.size()));
        parsed_rows.push_back(cols);
    }

    const int end_row = min(model_->rowCount() - 1, start.row() + static_cast<int>(parsed_rows.size()) - 1);
    const int end_col = min(model_->columnCount() - 1, start.column() + max_width - 1);
    const QItemSelectionRange target_range(model_->index(start.row(), start.column()), model_->index(end_row, end_col));
    UnmergeRangesIntersecting(target_range);

    for (int row_offset = 0; row_offset < parsed_rows.size(); ++row_offset) {
        const int row = start.row() + row_offset;
        if (row >= model_->rowCount()) break;
        const QStringList& cols = parsed_rows.at(row_offset);
        for (int col_offset = 0; col_offset < cols.size(); ++col_offset) {
            const int col = start.column() + col_offset;
            if (col >= model_->columnCount()) break;
            model_->setData(model_->index(row, col), cols.at(col_offset), Qt::EditRole);
        }
    }

    table_->selectionModel()->select(
        QItemSelection(model_->index(start.row(), start.column()), model_->index(end_row, end_col)),
        QItemSelectionModel::ClearAndSelect
    );
    table_->setCurrentIndex(model_->index(start.row(), start.column()));
    statusBar()->showMessage(QStringLiteral("\u5df2\u7c98\u8d34\u5230\u5f53\u524d\u533a\u57df"), 1500);
    UpdateSelectionInfo();
}

bool MainWindow::MergeSelectedCells() {
    const QItemSelectionRange range = CurrentSelectionRange();
    if (!range.isValid()) return false;

    const int row_span = range.bottom() - range.top() + 1;
    const int col_span = range.right() - range.left() + 1;
    if (row_span == 1 && col_span == 1) {
        statusBar()->showMessage(
            QStringLiteral("\u8bf7\u81f3\u5c11\u9009\u62e9\u4e24\u4e2a\u5355\u5143\u683c\u518d\u6267\u884c\u5408\u5e76"),
            1500
        );
        return false;
    }

    if (table_->HasInlineEditor()) {
        table_->CommitInlineEditor();
    }

    UnmergeRangesIntersecting(range);

    QModelIndexList to_clear;
    for (int row = range.top(); row <= range.bottom(); ++row) {
        for (int col = range.left(); col <= range.right(); ++col) {
            if (row == range.top() && col == range.left()) continue;
            to_clear.push_back(model_->index(row, col));
        }
    }
    if (!to_clear.isEmpty()) {
        model_->setData(to_clear, QString(), Qt::EditRole);
    }

    table_->setSpan(range.top(), range.left(), row_span, col_span);
    merged_ranges_.push_back(range);
    table_->setCurrentIndex(range.topLeft());
    statusBar()->showMessage(QStringLiteral("\u5df2\u5408\u5e76\u9009\u4e2d\u7684\u5355\u5143\u683c"), 1500);
    UpdateSelectionInfo();
    return true;
}

bool MainWindow::UnmergeSelectedCells() {
    QItemSelectionRange range = CurrentSelectionRange();
    if (!range.isValid()) {
        const int merged_index = FindMergedRangeContaining(table_->currentIndex());
        if (merged_index < 0) return false;
        range = merged_ranges_.at(merged_index);
    }

    const int previous_count = merged_ranges_.size();
    UnmergeRangesIntersecting(range);
    if (merged_ranges_.size() == previous_count) {
        statusBar()->showMessage(
            QStringLiteral("\u5f53\u524d\u9009\u533a\u4e2d\u6ca1\u6709\u5df2\u5408\u5e76\u7684\u5355\u5143\u683c"),
            1500
        );
        return false;
    }

    statusBar()->showMessage(QStringLiteral("\u5df2\u53d6\u6d88\u5408\u5e76\u5355\u5143\u683c"), 1500);
    UpdateSelectionInfo();
    return true;
}

void MainWindow::ShowTableContextMenu(const QPoint& pos) {
    QModelIndex clicked = table_->indexAt(pos);
    if (clicked.isValid() && table_->selectionModel() && !table_->selectionModel()->isSelected(clicked)) {
        table_->selectionModel()->select(clicked, QItemSelectionModel::ClearAndSelect);
        table_->setCurrentIndex(clicked);
    }

    QMenu menu(this);
    auto* cut_action = menu.addAction(QStringLiteral("\u526a\u5207"));
    auto* copy_action = menu.addAction(QStringLiteral("\u590d\u5236"));
    auto* paste_action = menu.addAction(QStringLiteral("\u7c98\u8d34"));
    menu.addSeparator();
    auto* clear_action = menu.addAction(QStringLiteral("\u6e05\u9664\u5185\u5bb9"));
    auto* text_color_action = menu.addAction(QStringLiteral("设置字体颜色"));
    auto* clear_text_color_action = menu.addAction(QStringLiteral("清除字体颜色"));
    auto* fill_color_action = menu.addAction(QStringLiteral("设置填充颜色"));
    auto* clear_fill_color_action = menu.addAction(QStringLiteral("清除填充颜色"));
    auto* sort_asc_action = menu.addAction(QStringLiteral("升序 A -> Z"));
    auto* sort_desc_action = menu.addAction(QStringLiteral("降序 Z -> A"));
    menu.addSeparator();
    auto* sum_action = menu.addAction(QStringLiteral("\u63d2\u5165 SUM"));
    auto* avg_action = menu.addAction(QStringLiteral("\u63d2\u5165 AVG"));
    auto* sin_action = menu.addAction(QStringLiteral("插入 SIN"));
    auto* cos_action = menu.addAction(QStringLiteral("插入 COS"));
    auto* count_action = menu.addAction(QStringLiteral("插入 COUNT"));
    auto* sqrt_action = menu.addAction(QStringLiteral("插入 SQRT"));
    auto* abs_action = menu.addAction(QStringLiteral("插入 ABS"));
    auto* round_action = menu.addAction(QStringLiteral("插入 ROUND"));
    auto* pow_action = menu.addAction(QStringLiteral("插入 POW"));
    menu.addSeparator();
    auto* line_chart_action = menu.addAction(QStringLiteral("\u6298\u7ebf\u56fe(Python)"));
    auto* bar_chart_action = menu.addAction(QStringLiteral("\u67f1\u72b6\u56fe(Python)"));
    auto* pie_chart_action = menu.addAction(QStringLiteral("\u997c\u56fe(Python)"));
    paste_action->setEnabled(!QApplication::clipboard()->text().isEmpty());

    QAction* chosen = menu.exec(table_->viewport()->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == cut_action) {
        CopySelectionToClipboard(true);
    } else if (chosen == copy_action) {
        CopySelectionToClipboard(false);
    } else if (chosen == paste_action) {
        PasteFromClipboard();
    } else if (chosen == clear_action) {
        ClearSelectedCells();
    } else if (chosen == text_color_action) {
        SetSelectedTextColor();
    } else if (chosen == clear_text_color_action) {
        ClearSelectedTextColor();
    } else if (chosen == fill_color_action) {
        SetSelectedFillColor();
    } else if (chosen == clear_fill_color_action) {
        ClearSelectedFillColor();
    } else if (chosen == sort_asc_action) {
        SortSelectedRange(true);
    } else if (chosen == sort_desc_action) {
        SortSelectedRange(false);
    } else if (chosen == sum_action) {
        InsertFunctionTemplate("SUM");
    } else if (chosen == avg_action) {
        InsertFunctionTemplate("AVG");
    } else if (chosen == sin_action) {
        InsertFunctionTemplate("SIN");
    } else if (chosen == cos_action) {
        InsertFunctionTemplate("COS");
    } else if (chosen == count_action) {
        InsertFunctionTemplate("COUNT");
    } else if (chosen == sqrt_action) {
        InsertFunctionTemplate("SQRT");
    } else if (chosen == abs_action) {
        InsertFunctionTemplate("ABS");
    } else if (chosen == round_action) {
        InsertFunctionTemplate("ROUND");
    } else if (chosen == pow_action) {
        InsertFunctionTemplate("POW");
    } else if (chosen == line_chart_action) {
        PlotSelectionWithPython("line");
    } else if (chosen == bar_chart_action) {
        PlotSelectionWithPython("bar");
    } else if (chosen == pie_chart_action) {
        PlotSelectionWithPython("pie");
    }
}

QString MainWindow::CurrentSelectionFormulaReference() const {
    const QModelIndexList indexes = SelectedIndexes();
    if (indexes.isEmpty()) return {};

    SelectionBounds bounds;
    if (!TryGetSelectionBounds(indexes, &bounds)) return {};
    return RangeReference(model_->index(bounds.min_row, bounds.min_col), model_->index(bounds.max_row, bounds.max_col));
}

void MainWindow::ResetCurrentSheet() {
    table_->clearSpans();
    merged_ranges_.clear();
    model_->clearAll();
    formula_bar_->clear();
    table_->clearSelection();
    table_->setCurrentIndex(model_->index(0, 0));
    current_sheet_rows_ = 1;
    current_sheet_cols_ = 1;
    current_document_path_.clear();
    EndFormulaEditingMode();
    statusBar()->showMessage(QStringLiteral("\u5df2\u65b0\u5efa\u7a7a\u767d\u8868\u683c"), 1500);
    UpdateSelectionInfo();
}

void MainWindow::ImportInFile() {
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("导入测试用例"),
        current_document_path_,
        QStringLiteral("输入文件 (*.in);;所有文件 (*.*)")
    );
    if (path.isEmpty()) return;

    emw::SpreadsheetGrid grid;
    int rows = 0;
    int cols = 0;
    if (!emw_app::LoadInToGrid(path.toStdString(), grid, rows, cols)) {
        QMessageBox::warning(
            this,
            QStringLiteral("导入失败"),
            QStringLiteral("无法读取 .in 文件，请检查首行行列数和后续逗号分隔内容。")
        );
        return;
    }

    emw::DatDocument document;
    document.rows = max(1, rows);
    document.cols = max(1, cols);

    grid.ForEachCell([&](const emw::Address& addr, const emw::Cell& cell) {
        if (cell.raw.empty()) return;

        emw::DatCellRecord record;
        record.addr = addr;
        record.raw = cell.raw;
        document.cells.push_back(::move(record));
    });

    ApplyDocument(document);
    current_document_path_.clear();
    statusBar()->showMessage(
        QStringLiteral("已导入测试用例：%1").arg(QFileInfo(path).fileName()),
        2000
    );
}

void MainWindow::OpenCsvFile() {
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("打开 CSV 文件"),
        current_document_path_,
        QStringLiteral("CSV 文件 (*.csv);;所有文件 (*.*)")
    );
    if (path.isEmpty()) return;
    OpenDocumentAsync(path, false);
}

void MainWindow::OpenDatFile() {
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("打开 DAT 文件"),
        current_document_path_,
        QStringLiteral("DAT 文件 (*.dat);;所有文件 (*.*)")
    );
    if (path.isEmpty()) return;
    OpenDocumentAsync(path, true);
}

void MainWindow::CompareStorageEfficiencyFiles() {
    const QString csv_path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("\u9009\u62e9 CSV \u6587\u4ef6"),
        current_document_path_,
        QStringLiteral("CSV 文件 (*.csv);;所有文件 (*.*)")
    );
    if (csv_path.isEmpty()) return;

    const QString dat_path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("\u9009\u62e9 DAT \u6587\u4ef6"),
        QFileInfo(csv_path).absolutePath(),
        QStringLiteral("DAT 文件 (*.dat);;所有文件 (*.*)")
    );
    if (dat_path.isEmpty()) return;

    const QFileInfo csv_info(csv_path);
    const QFileInfo dat_info(dat_path);
    if (!csv_info.exists() || !dat_info.exists() || csv_info.size() <= 0) {
        QMessageBox::warning(
            this,
            QStringLiteral("\u6bd4\u8f83\u5b58\u50a8\u6548\u7387"),
            QStringLiteral("\u6587\u4ef6\u4e0d\u5b58\u5728\uff0c\u6216 CSV \u6587\u4ef6\u5927\u5c0f\u4e3a 0\u3002")
        );
        return;
    }

    const vector<string> csv_paths{csv_path.toStdString()};
    const vector<string> dat_paths{dat_path.toStdString()};
    const double dat_ratio_percent = emw_app::CalculateStorageEfficiencyPercent(csv_paths, dat_paths);

    if (dat_ratio_percent <= 0.0) {
        QMessageBox::warning(
            this,
            QStringLiteral("\u6bd4\u8f83\u5b58\u50a8\u6548\u7387"),
            QStringLiteral("\u8ba1\u7b97\u5931\u8d25\uff0c\u8bf7\u68c0\u67e5\u9009\u62e9\u7684 CSV/DAT \u6587\u4ef6\u662f\u5426\u53ef\u8bfb\u3002")
        );
        return;
    }

    const double saving_percent = 100.0 - dat_ratio_percent;
    const QString summary = QStringLiteral(
                                "CSV: %1\n"
                                "DAT: %2\n\n"
                                "CSV \u5927\u5c0f: %3 bytes\n"
                                "DAT \u5927\u5c0f: %4 bytes\n\n"
                                "DAT/CSV: %5%\n"
                                "\u5b58\u50a8\u53d8\u5316: %6%"
                            )
                                .arg(csv_info.fileName())
                                .arg(dat_info.fileName())
                                .arg(csv_info.size())
                                .arg(dat_info.size())
                                .arg(QString::number(dat_ratio_percent, 'f', 2))
                                .arg(QString::number(saving_percent, 'f', 2));

    QMessageBox::information(this, QStringLiteral("\u5b58\u50a8\u6548\u7387\u6bd4\u8f83\u7ed3\u679c"), summary);
    statusBar()->showMessage(
        QStringLiteral("DAT/CSV 比例：%1%").arg(QString::number(dat_ratio_percent, 'f', 2)),
        3000
    );
}

void MainWindow::SaveFile() {
    if (current_document_path_.isEmpty()) {
        SaveFileAs();
        return;
    }

    if (!SaveFileByPath(current_document_path_)) {
        SaveFileAs();
    }
}

void MainWindow::SaveFileAs() {
    const QString default_path = current_document_path_.isEmpty() ? QStringLiteral("sheet.dat")
                                                                  : current_document_path_;
    QString selected_filter = QFileInfo(default_path).suffix().compare(QStringLiteral("csv"), Qt::CaseInsensitive) == 0
                                  ? QStringLiteral("CSV 文件 (*.csv)")
                                  : QStringLiteral("DAT 文件 (*.dat)");

    QString path = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("另存为"),
        default_path,
        QStringLiteral("DAT 文件 (*.dat);;CSV 文件 (*.csv);;所有文件 (*.*)"),
        &selected_filter
    );
    if (path.isEmpty()) return;

    if (QFileInfo(path).suffix().isEmpty()) {
        if (selected_filter.startsWith(QStringLiteral("CSV"), Qt::CaseInsensitive)) {
            path += QStringLiteral(".csv");
        } else {
            path += QStringLiteral(".dat");
        }
    }

    SaveFileByPath(path);
}

bool MainWindow::SaveFileByPath(const QString& path) {
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QStringLiteral("dat")) {
        return SaveDatFileAs(path);
    }
    if (suffix == QStringLiteral("csv")) {
        return SaveCsvFileAs(path);
    }

    QMessageBox::warning(
        this,
        QStringLiteral("保存失败"),
        QStringLiteral("暂不支持该文件类型，请选择 .csv 或 .dat。")
    );
    return false;
}

bool MainWindow::SaveDatFileAs(const QString& path) {
    if (table_->HasInlineEditor()) {
        table_->CommitInlineEditor();
    }

    const emw::DatDocument document = BuildCurrentDocument();
    if (!emw::DatFile::SaveDocument(path.toStdString(), document)) {
        QMessageBox::warning(
            this,
            QStringLiteral("保存失败"),
            QStringLiteral("无法写入 DAT 文件。")
        );
        return false;
    }

    current_document_path_ = path;
    statusBar()->showMessage(
        QStringLiteral("已保存：%1").arg(QFileInfo(path).fileName()),
        2000
    );
    return true;
}

bool MainWindow::SaveCsvFileAs(const QString& path) {
    if (table_->HasInlineEditor()) {
        table_->CommitInlineEditor();
    }

    const emw::SpreadsheetGrid& grid = model_->grid();
    if (!emw_app::WriteGridRawCsv(path.toStdString(), grid, current_sheet_rows_, current_sheet_cols_)) {
        QMessageBox::warning(
            this,
            QStringLiteral("保存失败"),
            QStringLiteral("无法写入 CSV 文件。")
        );
        return false;
    }

    current_document_path_ = path;
    statusBar()->showMessage(
        QStringLiteral("已保存：%1").arg(QFileInfo(path).fileName()),
        2000
    );
    return true;
}

emw::DatDocument MainWindow::BuildCurrentDocument() const {
    emw::DatDocument document;
    document.rows = max(1, current_sheet_rows_);
    document.cols = max(1, current_sheet_cols_);

    // Sparse serialization: keep only cells with content/style plus section metadata.
    unordered_map<int, emw::DatCellRecord> records;
    auto touch_bounds = [&document](int row, int col) {
        document.rows = max(document.rows, row + 1);
        document.cols = max(document.cols, col + 1);
    };

    const emw::SpreadsheetGrid& const_grid = model_->grid();
    auto& grid = const_cast<emw::SpreadsheetGrid&>(const_grid);
    grid.ForEachCell([&](const emw::Address& addr, const emw::Cell& cell) {
        touch_bounds(addr.row, addr.col);

        emw::DatCellRecord record;
        record.addr = addr;
        record.raw = cell.raw;
        records[addr.row * emw::kMaxCols + addr.col] = ::move(record);
    });

    for (const auto& entry : model_->styledCells()) {
        const emw::Address& addr = entry.first;
        const SpreadsheetModel::CellStyle& style = entry.second;
        touch_bounds(addr.row, addr.col);

        emw::DatCellRecord& record = records[addr.row * emw::kMaxCols + addr.col];
        record.addr = addr;
        record.style = ToDatStyle(style);
    }

    auto* horizontal = table_->horizontalHeader();
    auto* vertical = table_->verticalHeader();

    for (int col = 0; col < model_->columnCount(); ++col) {
        const int size = horizontal->sectionSize(col);
        if (size == kDefaultColumnWidth) continue;
        document.col_widths.push_back({col, size});
        touch_bounds(0, col);
    }

    for (int row = 0; row < model_->rowCount(); ++row) {
        const int size = vertical->sectionSize(row);
        if (size == kDefaultRowHeight) continue;
        document.row_heights.push_back({row, size});
        touch_bounds(row, 0);
    }

    for (const QItemSelectionRange& range : merged_ranges_) {
        if (!range.isValid()) continue;

        const int row_span = range.bottom() - range.top() + 1;
        const int col_span = range.right() - range.left() + 1;
        if (row_span <= 1 && col_span <= 1) continue;

        document.merged_ranges.push_back({
            emw::Address{range.top(), range.left()},
            row_span,
            col_span
        });
        touch_bounds(range.bottom(), range.right());
    }

    document.cells.reserve(records.size());
    for (auto& entry : records) {
        if (entry.second.raw.empty() && entry.second.style.IsDefault()) continue;
        document.cells.push_back(::move(entry.second));
    }

    sort(document.cells.begin(), document.cells.end(), [](const emw::DatCellRecord& lhs, const emw::DatCellRecord& rhs) {
        if (lhs.addr.row != rhs.addr.row) return lhs.addr.row < rhs.addr.row;
        return lhs.addr.col < rhs.addr.col;
    });

    return document;
}

void MainWindow::OpenDocumentAsync(const QString& path, bool is_dat) {
    if (open_in_progress_) {
        statusBar()->showMessage(QStringLiteral("正在加载其他文件，请稍候..."), 2000);
        return;
    }

    open_in_progress_ = true;
    table_->setEnabled(false);
    formula_bar_->setEnabled(false);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    statusBar()->showMessage(QStringLiteral("正在加载：%1").arg(QFileInfo(path).fileName()));

    const string std_path = path.toStdString();
    open_watcher_->setFuture(QtConcurrent::run([path, std_path, is_dat]() {
        OpenDocumentResult result;
        result.path = path;

        if (is_dat) {
            auto document = std::make_shared<emw::DatDocument>();
            string error;
            if (!emw::DatFile::LoadDocument(std_path, *document, &error)) {
                result.error = QStringLiteral("无法打开 DAT 文件：%1").arg(QString::fromStdString(error));
                return result;
            }
            result.document = ::move(document);
            result.prepared_grid = BuildGridFromDocument(*result.document);
            result.ok = true;
            return result;
        }

        emw::SpreadsheetGrid grid;
        int rows = 0;
        int cols = 0;
        if (!emw_app::LoadCsvToGrid(std_path, grid, rows, cols)) {
            result.error = QStringLiteral("无法打开 CSV 文件。");
            return result;
        }

        result.document = std::make_shared<emw::DatDocument>(BuildDocumentFromGrid(grid, rows, cols));
        result.prepared_grid = std::make_shared<emw::SpreadsheetGrid>(::move(grid));
        result.ok = true;
        return result;
    }));
}

void MainWindow::OnOpenDocumentFinished() {
    open_in_progress_ = false;
    table_->setEnabled(true);
    formula_bar_->setEnabled(true);
    QApplication::restoreOverrideCursor();

    const OpenDocumentResult result = open_watcher_->result();
    if (!result.ok || !result.document) {
        QMessageBox::warning(
            this,
            QStringLiteral("打开失败"),
            result.error.isEmpty() ? QStringLiteral("无法打开文件。") : result.error
        );
        statusBar()->showMessage(QStringLiteral("打开失败"), 2000);
        return;
    }

    ApplyDocument(*result.document, result.prepared_grid);
    current_document_path_ = result.path;
    statusBar()->showMessage(QStringLiteral("已打开：%1").arg(QFileInfo(result.path).fileName()), 2000);
}

void MainWindow::ApplyDocument(const emw::DatDocument& document) {
    ApplyDocument(document, nullptr);
}

void MainWindow::ApplyDocument(const emw::DatDocument& document, std::shared_ptr<emw::SpreadsheetGrid> prepared_grid) {
    if (table_->HasInlineEditor()) {
        table_->CommitInlineEditor();
    }

    emw::SpreadsheetGrid grid;
    if (prepared_grid) {
        grid = ::move(*prepared_grid);
    } else {
        for (const emw::DatCellRecord& record : document.cells) {
            if (!record.addr.is_valid()) continue;
            grid.SetCell(record.addr, record.raw, nullptr);
        }
    }

    unordered_map<int, SpreadsheetModel::CellStyle> styles;
    styles.reserve(document.cells.size());
    for (const emw::DatCellRecord& record : document.cells) {
        if (record.style.IsDefault()) continue;
        if (!record.addr.is_valid()) continue;
        styles[record.addr.row * emw::kMaxCols + record.addr.col] = ToModelStyle(record.style);
    }

    table_->clearSpans();
    merged_ranges_.clear();
    model_->loadFromGrid(::move(grid), ::move(styles));

    current_sheet_rows_ = max(1, document.rows);
    current_sheet_cols_ = max(1, document.cols);

    auto* horizontal = table_->horizontalHeader();
    auto* vertical = table_->verticalHeader();

    for (int col : resized_col_sections_) {
        if (col < 0 || col >= model_->columnCount()) continue;
        horizontal->resizeSection(col, kDefaultColumnWidth);
    }
    for (int row : resized_row_sections_) {
        if (row < 0 || row >= model_->rowCount()) continue;
        vertical->resizeSection(row, kDefaultRowHeight);
    }
    resized_col_sections_.clear();
    resized_row_sections_.clear();

    for (const emw::DatSizedSection& section : document.col_widths) {
        if (section.index < 0 || section.index >= model_->columnCount()) continue;
        if (section.size <= 0) continue;
        horizontal->resizeSection(section.index, section.size);
        if (section.size != kDefaultColumnWidth) {
            resized_col_sections_.insert(section.index);
        }
    }
    for (const emw::DatSizedSection& section : document.row_heights) {
        if (section.index < 0 || section.index >= model_->rowCount()) continue;
        if (section.size <= 0) continue;
        vertical->resizeSection(section.index, section.size);
        if (section.size != kDefaultRowHeight) {
            resized_row_sections_.insert(section.index);
        }
    }

    for (const emw::DatMergeRange& range : document.merged_ranges) {
        if (!range.top_left.is_valid()) continue;
        if (range.row_span <= 1 && range.col_span <= 1) continue;
        if (range.top_left.row + range.row_span > model_->rowCount()) continue;
        if (range.top_left.col + range.col_span > model_->columnCount()) continue;

        table_->setSpan(range.top_left.row, range.top_left.col, range.row_span, range.col_span);
        merged_ranges_.push_back(QItemSelectionRange(
            model_->index(range.top_left.row, range.top_left.col),
            model_->index(range.top_left.row + range.row_span - 1, range.top_left.col + range.col_span - 1)
        ));
    }

    formula_bar_->clear();
    EndFormulaEditingMode();
    table_->clearSelection();
    table_->setCurrentIndex(model_->index(0, 0));
    table_->setFocus();
    UpdateSelectionInfo();
}

QString MainWindow::SelectionReferenceText() const {
    const QModelIndexList indexes = SelectedIndexes();
    if (indexes.isEmpty()) return QStringLiteral("\u672a\u9009\u62e9");

    if (!table_->selectionModel()) {
        return IndexReference(indexes.front());
    }

    const QItemSelection selection = table_->selectionModel()->selection();
    if (selection.size() == 1) {
        return RangeReference(selection.front());
    }

    return QStringLiteral("%1 \u4e2a\u533a\u57df\uff0c%2 \u4e2a\u5355\u5143\u683c").arg(selection.size()).arg(indexes.size());
}

QString MainWindow::ActiveFormulaText() const {
    if (formula_editor_source_ == FormulaEditorSource::InlineCell && table_->HasInlineEditor()) {
        return table_->InlineEditorText();
    }
    return formula_bar_->text();
}

int MainWindow::ActiveFormulaCursorPosition() const {
    if (formula_editor_source_ == FormulaEditorSource::InlineCell && table_->HasInlineEditor()) {
        return table_->InlineEditorCursorPosition();
    }
    return formula_bar_->cursorPosition();
}

int MainWindow::ActiveFormulaSelectionStart() const {
    if (formula_editor_source_ == FormulaEditorSource::InlineCell && table_->HasInlineEditor()) {
        return table_->InlineEditorSelectionStart();
    }
    return formula_bar_->selectionStart();
}

int MainWindow::ActiveFormulaSelectionLength() const {
    if (formula_editor_source_ == FormulaEditorSource::InlineCell && table_->HasInlineEditor()) {
        return table_->InlineEditorSelectionLength();
    }

    const int start = formula_bar_->selectionStart();
    return start >= 0 ? formula_bar_->selectedText().size() : 0;
}

void MainWindow::SetActiveFormulaText(const QString& text) {
    internal_formula_update_ = true;
    formula_bar_->setText(text);
    internal_formula_update_ = false;

    if (table_->HasInlineEditor()) {
        internal_inline_sync_ = true;
        table_->SetInlineEditorText(text);
        internal_inline_sync_ = false;
    }
}

void MainWindow::SetActiveFormulaCursorPosition(int position) {
    const int clamped = max(0, position);

    if (table_->HasInlineEditor()) {
        internal_inline_sync_ = true;
        table_->SetInlineEditorCursorPosition(clamped);
        internal_inline_sync_ = false;
    }

    internal_formula_update_ = true;
    formula_bar_->setCursorPosition(clamped);
    internal_formula_update_ = false;
}

void MainWindow::SyncFormulaBarFromInlineEditor() {
    if (!table_->HasInlineEditor()) return;
    internal_formula_update_ = true;
    formula_bar_->setText(table_->InlineEditorText());
    internal_formula_update_ = false;
}

void MainWindow::SyncInlineEditorFromFormulaBar() {
    if (!table_->HasInlineEditor()) return;
    internal_inline_sync_ = true;
    table_->SetInlineEditorText(formula_bar_->text());
    const int selection_start = formula_bar_->selectionStart();
    if (selection_start >= 0) {
        table_->SetInlineEditorSelection(selection_start, formula_bar_->selectedText().size());
    } else {
        table_->SetInlineEditorCursorPosition(formula_bar_->cursorPosition());
    }
    internal_inline_sync_ = false;
}

void MainWindow::UpdateSelectionInfo(bool sync_formula_bar) {
    const QModelIndexList indexes = SelectedIndexes();

    if (indexes.isEmpty()) {
        name_box_label_->setText("--");
        selection_summary_label_->setText(
            QStringLiteral(
                "\u5355\u51fb\u5355\u5143\u683c\u5f00\u59cb\u7f16\u8f91\uff0c\u62d6\u52a8\u9f20\u6807\u53ef\u4ee5\u8fde\u7eed\u9009\u62e9\u591a\u4e2a\u5355\u5143\u683c\u3002"
            )
        );
        shortcut_hint_label_->setText(QStringLiteral("Shift \u8fde\u9009 | Ctrl \u591a\u9009 | Delete \u6e05\u7a7a"));
        position_label_->setText(QStringLiteral("\u9009\u533a\uff1a\u672a\u9009\u62e9"));
        value_type_label_->setText(QStringLiteral("\u7c7b\u578b\uff1a\u7a7a\u503c"));
        if (sync_formula_bar) {
            formula_bar_->clear();
            formula_bar_->setPlaceholderText(DefaultFormulaPlaceholder());
        }
        return;
    }

    const QString reference = SelectionReferenceText();
    name_box_label_->setText(reference);
    position_label_->setText(QStringLiteral("\u9009\u533a\uff1a%1").arg(reference));

    QModelIndex current = table_->currentIndex().isValid() ? table_->currentIndex() : indexes.front();
    QString raw_text = model_->data(current, Qt::EditRole).toString();

    if (table_->HasInlineEditor()) {
        current = table_->InlineEditIndex();
        raw_text = table_->InlineEditorText();
    }

    if (sync_formula_bar) {
        formula_bar_->setPlaceholderText(DefaultFormulaPlaceholder());
        if (table_->HasInlineEditor()) {
            SyncFormulaBarFromInlineEditor();
        } else if (indexes.size() == 1) {
            formula_bar_->setText(raw_text);
        } else {
            formula_bar_->clear();
            formula_bar_->setPlaceholderText(
                QStringLiteral("\u5c06\u8f93\u5165\u5185\u5bb9\u6279\u91cf\u586b\u5145\u5230 %1 \u4e2a\u5df2\u9009\u5355\u5143\u683c")
                    .arg(indexes.size())
            );
        }
    }

    if (indexes.size() == 1) {
        selection_summary_label_->setText(
            QStringLiteral(
                "\u5f53\u524d\u5355\u5143\u683c %1\uff0c\u53ef\u76f4\u63a5\u8f93\u5165\u5185\u5bb9\uff0c\u6216\u5728\u516c\u5f0f\u680f\u4e2d\u7f16\u8f91\u3002"
            ).arg(reference)
        );
        shortcut_hint_label_->setText(
            IsFormulaEditingMode()
                ? QStringLiteral(
                      "\u516c\u5f0f\u6a21\u5f0f\uff1a\u70b9\u51fb\u6216\u62d6\u9009\u5355\u5143\u683c\uff0c\u4f1a\u66ff\u6362\u516c\u5f0f\u4e2d\u7684\u5f15\u7528"
                  )
                : QStringLiteral(
                      "\u62d6\u52a8\u53f3\u4e0b\u89d2\u586b\u5145\u67c4\u53ef\u81ea\u52a8\u586b\u5145 | \u53cc\u51fb\u6216\u76f4\u63a5\u8f93\u5165\u5f00\u59cb\u7f16\u8f91"
                  )
        );

        if (raw_text.isEmpty()) {
            value_type_label_->setText(QStringLiteral("\u7c7b\u578b\uff1a\u7a7a\u503c"));
        } else if (raw_text.startsWith('=')) {
            value_type_label_->setText(QStringLiteral("\u7c7b\u578b\uff1a\u516c\u5f0f"));
        } else {
            bool is_number = false;
            raw_text.toDouble(&is_number);
            value_type_label_->setText(
                is_number ? QStringLiteral("\u7c7b\u578b\uff1a\u6570\u5b57")
                          : QStringLiteral("\u7c7b\u578b\uff1a\u6587\u672c")
            );
        }
        return;
    }

    int numeric_count = 0;
    int text_count = 0;
    int empty_count = 0;
    int formula_count = 0;
    int error_count = 0;
    double numeric_sum = 0.0;

    for (const QModelIndex& index : indexes) {
        const QString raw = model_->data(index, Qt::EditRole).toString();
        if (raw.startsWith('=')) {
            formula_count++;
        }

        const emw::EvaluatedCell evaluated = model_->grid().GetEvaluatedCell(emw::Address{index.row(), index.column()});
        const emw::Value& value = evaluated.value;
        if (value.is_number()) {
            numeric_count++;
            numeric_sum += value.number;
        } else if (value.is_text()) {
            text_count++;
        } else if (value.is_empty()) {
            empty_count++;
        }

        if (evaluated.has_error) {
            error_count++;
        }
    }

    selection_summary_label_->setText(
        QStringLiteral(
            "\u5df2\u9009\u62e9 %1 \u4e2a\u5355\u5143\u683c\uff0c\u53ef\u6279\u91cf\u586b\u5145\u5185\u5bb9\uff0c\u4e5f\u53ef\u4f5c\u4e3a\u516c\u5f0f\u5f15\u7528\u533a\u57df\u3002"
        ).arg(indexes.size())
    );
    shortcut_hint_label_->setText(
        IsFormulaEditingMode()
            ? QStringLiteral(
                  "\u516c\u5f0f\u6a21\u5f0f\uff1a\u62d6\u9009\u533a\u57df\u4f1a\u5b9e\u65f6\u5199\u6210 A1:B6 \u8fd9\u6837\u7684\u5f15\u7528"
              )
            : QStringLiteral("Shift \u8fde\u9009 | Ctrl \u591a\u9009 | \u62d6\u52a8\u586b\u5145\u67c4\u5feb\u901f\u586b\u5145")
    );

    QStringList summary_parts;
    summary_parts << QStringLiteral("\u5df2\u9009 %1").arg(indexes.size());
    if (numeric_count > 0) {
        summary_parts << QStringLiteral("\u6570\u5b57 %1").arg(numeric_count);
        summary_parts << QStringLiteral("\u6c42\u548c %1").arg(FormatNumber(numeric_sum));
    }
    if (text_count > 0) summary_parts << QStringLiteral("\u6587\u672c %1").arg(text_count);
    if (formula_count > 0) summary_parts << QStringLiteral("\u516c\u5f0f %1").arg(formula_count);
    if (empty_count > 0) summary_parts << QStringLiteral("\u7a7a\u503c %1").arg(empty_count);
    if (error_count > 0) summary_parts << QStringLiteral("\u9519\u8bef %1").arg(error_count);
    value_type_label_->setText(summary_parts.join(QStringLiteral(" | ")));
}

void MainWindow::ApplyFormulaBarToCurrentCell() {
    if (table_->HasInlineEditor()) {
        if (formula_editor_source_ == FormulaEditorSource::FormulaBar) {
            SyncInlineEditorFromFormulaBar();
        }
        table_->CommitInlineEditor();
        return;
    }

    if (IsFormulaEditingMode()) {
        const QModelIndex target = ResolveFormulaTargetIndex();
        if (!target.isValid()) return;

        model_->setData(target, formula_bar_->text(), Qt::EditRole);
        EndFormulaEditingMode();
        table_->setCurrentIndex(target);
        table_->setFocus();
        statusBar()->showMessage(QStringLiteral("\u5df2\u66f4\u65b0 %1").arg(IndexReference(target)), 1500);
        UpdateSelectionInfo();
        return;
    }

    QModelIndexList targets = SelectedIndexes();
    if (targets.isEmpty()) {
        const QModelIndex target = ResolveFormulaTargetIndex();
        if (target.isValid()) targets.push_back(target);
    }
    if (targets.isEmpty()) return;

    const bool is_batch = targets.size() > 1;
    if (is_batch) {
        model_->setData(targets, formula_bar_->text(), Qt::EditRole);
    } else {
        model_->setData(targets.front(), formula_bar_->text(), Qt::EditRole);
    }

    table_->setCurrentIndex(targets.front());
    table_->setFocus();
    statusBar()->showMessage(
        is_batch ? QStringLiteral("\u5df2\u586b\u5145 %1 \u4e2a\u5355\u5143\u683c").arg(targets.size())
                 : QStringLiteral("\u5df2\u66f4\u65b0 %1").arg(IndexReference(targets.front())),
        1500
    );
    UpdateSelectionInfo(!is_batch);
}

bool MainWindow::IsFormulaEditingMode() const {
    return formula_editing_ && editing_index_.isValid() && ActiveFormulaText().startsWith('=');
}

void MainWindow::StartFormulaEditingMode() {
    QModelIndex current = table_->currentIndex();
    if (table_->HasInlineEditor()) {
        current = table_->InlineEditIndex();
    }
    if (!current.isValid()) return;
    if (!formula_editing_) {
        editing_index_ = current;
    }
    formula_editing_ = true;
    if (formula_editor_source_ == FormulaEditorSource::None) {
        formula_editor_source_ = table_->HasInlineEditor() ? FormulaEditorSource::InlineCell
                                                           : FormulaEditorSource::FormulaBar;
    }

    AnimateShadow(formula_shadow_, 48, QPointF(0, 18), this);
}

void MainWindow::EndFormulaEditingMode() {
    if (!formula_editing_) return;
    formula_editing_ = false;
    editing_index_ = QModelIndex();
    ClearFormulaReferenceTracking();
    formula_editor_source_ = FormulaEditorSource::None;

    AnimateShadow(formula_shadow_, 26, QPointF(0, 10), this);
}

QModelIndex MainWindow::ResolveFormulaTargetIndex() const {
    if (table_->HasInlineEditor()) return table_->InlineEditIndex();
    if (IsFormulaEditingMode()) return editing_index_;
    return table_->currentIndex();
}

bool MainWindow::ShouldCaptureSelectionAsFormulaReference() const {
    if (!IsFormulaEditingMode()) return false;

    const QString text = ActiveFormulaText();
    if (!text.startsWith('=')) return false;

    if (ActiveFormulaCursorPosition() < 0) return true;

    if (formula_reference_active_) return true;

    const int selection_start = ActiveFormulaSelectionStart();
    if (selection_start >= 0 && ActiveFormulaSelectionLength() > 0) {
        return true;
    }

    const int cursor = clamp(ActiveFormulaCursorPosition(), 0, static_cast<int>(text.size()));
    if (cursor <= 1) return true;

    const QChar prev = text.at(cursor - 1);
    const QString trigger_chars = "=,(+-*/:%";
    if (trigger_chars.contains(prev)) {
        return true;
    }

    return false;
}

void MainWindow::CommitActiveFormulaEditToCell() {
    const QModelIndex target = ResolveFormulaTargetIndex();
    const QString text = ActiveFormulaText();
    if (!target.isValid()) {
        EndFormulaEditingMode();
        return;
    }

    if (table_->HasInlineEditor()) {
        table_->CancelInlineEditor();
    }

    model_->setData(target, text, Qt::EditRole);
    EndFormulaEditingMode();
}

void MainWindow::UpdateFormulaReferenceFromSelection() {
    if (!IsFormulaEditingMode()) return;

    const QString reference = CurrentSelectionFormulaReference();
    if (reference.isEmpty()) return;

    ReplaceFormulaReference(reference);
}

void MainWindow::ReplaceFormulaReference(const QString& reference) {
    QString text = ActiveFormulaText();
    if (!text.startsWith('=')) return;
    const int text_size = static_cast<int>(text.size());

    int replace_start = formula_reference_start_;
    int replace_length = formula_reference_length_;

    if (!formula_reference_active_ || replace_start < 0 || replace_start > text_size) {
        const int selection_start = ActiveFormulaSelectionStart();
        if (selection_start >= 0) {
            replace_start = selection_start;
            replace_length = ActiveFormulaSelectionLength();
        } else {
            replace_start = ActiveFormulaCursorPosition();
            replace_length = 0;
        }
    }

    if (replace_start < 0) {
        replace_start = text_size;
        replace_length = 0;
    }
    if (replace_start <= 1 && text_size > 1) {
        replace_start = text_size;
        replace_length = 0;
    }

    replace_start = clamp(replace_start, 0, text_size);
    replace_length = clamp(replace_length, 0, text_size - replace_start);

    const QString updated = text.left(replace_start) + reference + text.mid(replace_start + replace_length);

    SetActiveFormulaText(updated);
    SetActiveFormulaCursorPosition(replace_start + reference.size());

    formula_reference_active_ = true;
    formula_reference_start_ = replace_start;
    formula_reference_length_ = reference.size();
}

void MainWindow::ClearFormulaReferenceTracking() {
    formula_reference_active_ = false;
    formula_reference_start_ = -1;
    formula_reference_length_ = 0;
}

void MainWindow::InsertFunctionTemplate(const QString& function_name) {
    const QModelIndex current = table_->currentIndex();
    if (!current.isValid()) return;

    editing_index_ = current;
    formula_editor_source_ = FormulaEditorSource::InlineCell;
    formula_editing_ = true;
    ClearFormulaReferenceTracking();

    const QString text = QString("=%1()").arg(function_name);
    table_->StartInlineEdit(current, text, true);
    table_->SetInlineEditorCursorPosition(text.size() - 1);
    table_->FocusInlineEditor();
    SyncFormulaBarFromInlineEditor();

    statusBar()->showMessage(QStringLiteral("\u5df2\u63d2\u5165 %1 \u6a21\u677f").arg(function_name), 1500);
    UpdateSelectionInfo(false);
}

void MainWindow::ApplyAutoFill(const QItemSelectionRange& source, const QItemSelectionRange& target) {
    if (!source.isValid() || !target.isValid()) return;
    if (table_->HasInlineEditor()) {
        table_->CommitInlineEditor();
    }
    UnmergeRangesIntersecting(target);

    const int source_height = source.bottom() - source.top() + 1;
    const int source_width = source.right() - source.left() + 1;
    const bool vertical_fill = source.left() == target.left() && source.right() == target.right();
    const bool horizontal_fill = source.top() == target.top() && source.bottom() == target.bottom();
    if (!vertical_fill && !horizontal_fill) return;

    if (vertical_fill) {
        for (int col_offset = 0; col_offset < source_width; ++col_offset) {
            QStringList source_values;
            QVector<double> numeric_values;
            bool numeric_pattern = source_height >= 2;
            double step = 0.0;

            for (int row_offset = 0; row_offset < source_height; ++row_offset) {
                const QModelIndex index = model_->index(source.top() + row_offset, source.left() + col_offset);
                const QString raw = model_->data(index, Qt::EditRole).toString();
                source_values << raw;

                double number = 0.0;
                if (!TryParsePlainNumber(raw, &number)) {
                    numeric_pattern = false;
                } else {
                    numeric_values.push_back(number);
                }
            }

            if (numeric_pattern) {
                step = numeric_values[1] - numeric_values[0];
                for (int i = 2; i < numeric_values.size(); ++i) {
                    if (abs((numeric_values[i] - numeric_values[i - 1]) - step) > 1e-9) {
                        numeric_pattern = false;
                        break;
                    }
                }
            }

            for (int row = target.top(); row <= target.bottom(); ++row) {
                const int relative_offset = row - source.top();
                const QString filled_value = numeric_pattern
                                                 ? FormatAutoFillNumber(numeric_values[0] + step * relative_offset)
                                                 : source_values[PositiveModulo(relative_offset, source_height)];
                model_->setData(model_->index(row, source.left() + col_offset), filled_value, Qt::EditRole);
            }
        }
    } else {
        for (int row_offset = 0; row_offset < source_height; ++row_offset) {
            QStringList source_values;
            QVector<double> numeric_values;
            bool numeric_pattern = source_width >= 2;
            double step = 0.0;

            for (int col_offset = 0; col_offset < source_width; ++col_offset) {
                const QModelIndex index = model_->index(source.top() + row_offset, source.left() + col_offset);
                const QString raw = model_->data(index, Qt::EditRole).toString();
                source_values << raw;

                double number = 0.0;
                if (!TryParsePlainNumber(raw, &number)) {
                    numeric_pattern = false;
                } else {
                    numeric_values.push_back(number);
                }
            }

            if (numeric_pattern) {
                step = numeric_values[1] - numeric_values[0];
                for (int i = 2; i < numeric_values.size(); ++i) {
                    if (abs((numeric_values[i] - numeric_values[i - 1]) - step) > 1e-9) {
                        numeric_pattern = false;
                        break;
                    }
                }
            }

            for (int col = target.left(); col <= target.right(); ++col) {
                const int relative_offset = col - source.left();
                const QString filled_value = numeric_pattern
                                                 ? FormatAutoFillNumber(numeric_values[0] + step * relative_offset)
                                                 : source_values[PositiveModulo(relative_offset, source_width)];
                model_->setData(model_->index(source.top() + row_offset, col), filled_value, Qt::EditRole);
            }
        }
    }

    const QModelIndex top_left = model_->index(min(source.top(), target.top()), min(source.left(), target.left()));
    const QModelIndex bottom_right =
        model_->index(max(source.bottom(), target.bottom()), max(source.right(), target.right()));
    table_->selectionModel()->select(
        QItemSelection(top_left, bottom_right),
        QItemSelectionModel::ClearAndSelect
    );
    table_->setCurrentIndex(top_left);
    statusBar()->showMessage(QStringLiteral("\u5df2\u5b8c\u6210\u81ea\u52a8\u586b\u5145"), 1500);
    UpdateSelectionInfo();
}

void MainWindow::ResizeSelectedCells(int column_delta, int row_delta) {
    const QModelIndexList indexes = SelectedIndexes();
    if (indexes.isEmpty()) return;

    set<int> rows;
    set<int> cols;
    for (const QModelIndex& index : indexes) {
        rows.insert(index.row());
        cols.insert(index.column());
    }

    auto* horizontal = table_->horizontalHeader();
    auto* vertical = table_->verticalHeader();

    for (int col : cols) {
        const int next_width = max(horizontal->minimumSectionSize(), horizontal->sectionSize(col) + column_delta);
        horizontal->resizeSection(col, next_width);
    }

    for (int row : rows) {
        const int next_height = max(vertical->minimumSectionSize(), vertical->sectionSize(row) + row_delta);
        vertical->resizeSection(row, next_height);
    }

    statusBar()->showMessage(QStringLiteral("\u5df2\u8c03\u6574\u5f53\u524d\u9009\u533a\u7684\u5355\u5143\u683c\u5927\u5c0f"), 1500);
}

void MainWindow::ResetSelectedCellSizes() {
    const QModelIndexList indexes = SelectedIndexes();
    if (indexes.isEmpty()) return;

    set<int> rows;
    set<int> cols;
    for (const QModelIndex& index : indexes) {
        rows.insert(index.row());
        cols.insert(index.column());
    }

    auto* horizontal = table_->horizontalHeader();
    auto* vertical = table_->verticalHeader();

    for (int col : cols) {
        horizontal->resizeSection(col, kDefaultColumnWidth);
    }

    for (int row : rows) {
        vertical->resizeSection(row, kDefaultRowHeight);
    }

    statusBar()->showMessage(QStringLiteral("\u5df2\u6062\u590d\u9ed8\u8ba4\u5355\u5143\u683c\u5927\u5c0f"), 1500);
}

void MainWindow::SetSelectedTextColor() {
    if (table_->HasInlineEditor()) {
        table_->CommitInlineEditor();
    }

    const QModelIndexList indexes = SelectedIndexes();
    if (indexes.isEmpty()) return;

    QColor initial_color = last_text_color_.isValid() ? last_text_color_ : QColor(QStringLiteral("#217346"));
    for (const QModelIndex& index : indexes) {
        const SpreadsheetModel::CellStyle* style = model_->cellStyle(index);
        if (style && style->has_foreground && style->foreground.isValid()) {
            initial_color = style->foreground;
            break;
        }
    }

    const QColor color = PickBasicColor(this, initial_color, QStringLiteral("选择字体颜色"));
    if (!color.isValid()) return;
    last_text_color_ = color;

    for (const QModelIndex& index : indexes) {
        SpreadsheetModel::CellStyle style;
        if (const SpreadsheetModel::CellStyle* existing = model_->cellStyle(index)) {
            style = *existing;
        }
        style.has_foreground = true;
        style.foreground = color;
        model_->setCellStyle(index, style);
    }

    statusBar()->showMessage(QStringLiteral("已设置选区字体颜色"), 1500);
    UpdateSelectionInfo(!IsFormulaEditingMode());
}

void MainWindow::ClearSelectedTextColor() {
    if (table_->HasInlineEditor()) {
        table_->CommitInlineEditor();
    }

    const QModelIndexList indexes = SelectedIndexes();
    if (indexes.isEmpty()) return;

    bool changed = false;
    for (const QModelIndex& index : indexes) {
        const SpreadsheetModel::CellStyle* existing = model_->cellStyle(index);
        if (!existing || !existing->has_foreground) continue;

        SpreadsheetModel::CellStyle style = *existing;
        style.has_foreground = false;
        style.foreground = QColor();
        model_->setCellStyle(index, style);
        changed = true;
    }

    if (!changed) return;

    statusBar()->showMessage(QStringLiteral("已清除选区字体颜色"), 1500);
    UpdateSelectionInfo(!IsFormulaEditingMode());
}

void MainWindow::SetSelectedFillColor() {
    if (table_->HasInlineEditor()) {
        table_->CommitInlineEditor();
    }

    const QModelIndexList indexes = SelectedIndexes();
    if (indexes.isEmpty()) return;

    QColor initial_color = last_fill_color_.isValid() ? last_fill_color_ : QColor(QStringLiteral("#d8ead9"));
    for (const QModelIndex& index : indexes) {
        const SpreadsheetModel::CellStyle* style = model_->cellStyle(index);
        if (style && style->has_background && style->background.isValid()) {
            initial_color = style->background;
            break;
        }
    }

    const QColor color = PickBasicColor(this, initial_color, QStringLiteral("选择填充颜色"));
    if (!color.isValid()) return;
    last_fill_color_ = color;

    for (const QModelIndex& index : indexes) {
        SpreadsheetModel::CellStyle style;
        if (const SpreadsheetModel::CellStyle* existing = model_->cellStyle(index)) {
            style = *existing;
        }
        style.has_background = true;
        style.background = color;
        model_->setCellStyle(index, style);
    }

    statusBar()->showMessage(QStringLiteral("已设置选区填充颜色"), 1500);
    UpdateSelectionInfo(!IsFormulaEditingMode());
}

void MainWindow::ClearSelectedFillColor() {
    if (table_->HasInlineEditor()) {
        table_->CommitInlineEditor();
    }

    const QModelIndexList indexes = SelectedIndexes();
    if (indexes.isEmpty()) return;

    bool changed = false;
    for (const QModelIndex& index : indexes) {
        const SpreadsheetModel::CellStyle* existing = model_->cellStyle(index);
        if (!existing || !existing->has_background) continue;

        SpreadsheetModel::CellStyle style = *existing;
        style.has_background = false;
        style.background = QColor();
        model_->setCellStyle(index, style);
        changed = true;
    }

    if (!changed) return;

    statusBar()->showMessage(QStringLiteral("已清除选区填充颜色"), 1500);
    UpdateSelectionInfo(!IsFormulaEditingMode());
}

void MainWindow::SortSelectedRange(bool ascending) {
    if (table_->HasInlineEditor()) {
        table_->CommitInlineEditor();
    }

    const QItemSelectionRange range = CurrentSelectionRange();
    if (!range.isValid()) return;
    if (range.bottom() <= range.top()) {
        statusBar()->showMessage(QStringLiteral("排序至少需要两行数据"), 1500);
        return;
    }

    UnmergeRangesIntersecting(range);

    struct RowSnapshot {
        int original_row = 0;
        SortCellKey key;
        QVector<QString> raw_values;
        QVector<SpreadsheetModel::CellStyle> styles;
    };

    vector<RowSnapshot> rows;
    rows.reserve(static_cast<size_t>(range.bottom() - range.top() + 1));

    const int sort_col = range.left();
    const int width = range.right() - range.left() + 1;
    for (int row = range.top(); row <= range.bottom(); ++row) {
        RowSnapshot snapshot;
        snapshot.original_row = row;
        snapshot.key = BuildSortCellKey(model_->data(model_->index(row, sort_col), Qt::DisplayRole).toString());
        snapshot.raw_values.reserve(width);
        snapshot.styles.reserve(width);

        for (int col = range.left(); col <= range.right(); ++col) {
            const QModelIndex index = model_->index(row, col);
            snapshot.raw_values.push_back(model_->data(index, Qt::EditRole).toString());
            SpreadsheetModel::CellStyle style;
            if (const SpreadsheetModel::CellStyle* existing = model_->cellStyle(index)) {
                style = *existing;
            }
            snapshot.styles.push_back(style);
        }
        rows.push_back(std::move(snapshot));
    }

    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);
    collator.setIgnorePunctuation(false);

    stable_sort(rows.begin(), rows.end(), [ascending, &collator](const RowSnapshot& lhs, const RowSnapshot& rhs) {
        const int cmp = CompareSortKey(lhs.key, rhs.key, ascending, &collator);
        if (cmp == 0) return lhs.original_row < rhs.original_row;
        return cmp < 0;
    });

    for (int offset = 0; offset < static_cast<int>(rows.size()); ++offset) {
        const int row = range.top() + offset;
        const RowSnapshot& snapshot = rows[static_cast<size_t>(offset)];
        for (int col_offset = 0; col_offset < width; ++col_offset) {
            const QModelIndex index = model_->index(row, range.left() + col_offset);
            model_->setData(index, snapshot.raw_values[col_offset], Qt::EditRole);
            model_->setCellStyle(index, snapshot.styles[col_offset]);
        }
    }

    if (table_->selectionModel()) {
        table_->selectionModel()->select(
            QItemSelection(model_->index(range.top(), range.left()), model_->index(range.bottom(), range.right())),
            QItemSelectionModel::ClearAndSelect
        );
    }
    table_->setCurrentIndex(model_->index(range.top(), range.left()));
    statusBar()->showMessage(
        ascending ? QStringLiteral("已按首列升序排序选区") : QStringLiteral("已按首列降序排序选区"),
        1500
    );
    UpdateSelectionInfo(!IsFormulaEditingMode());
}

void MainWindow::ClearSelectedCells() {
    if (table_->HasInlineEditor()) {
        table_->CommitInlineEditor();
    }

    const QModelIndexList indexes = SelectedIndexes();
    if (indexes.isEmpty()) return;

    model_->setData(indexes, QString(), Qt::EditRole);
    if (!IsFormulaEditingMode()) {
        UpdateSelectionInfo();
    } else {
        UpdateSelectionInfo(false);
    }
    statusBar()->showMessage(QStringLiteral("\u5df2\u6e05\u7a7a\u9009\u533a"), 1500);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if ((watched == table_ || watched == table_->viewport()) && event && event->type() == QEvent::KeyPress) {
        if (IsFormulaEditingMode()) {
            auto* key_event = static_cast<QKeyEvent*>(event);
            const QString text = key_event->text();
            if (!text.isEmpty()) {
                const QChar ch = text.at(0);
                const QString ops = "+-*/%";
                if (ops.contains(ch)) {
                    QString formula = ActiveFormulaText();
                    if (!formula.startsWith('=')) {
                        formula = "=" + formula;
                    }
                    formula += ch;
                    SetActiveFormulaText(formula);
                    SetActiveFormulaCursorPosition(formula.size());
                    ClearFormulaReferenceTracking();
                    return true;
                }
            }
        }
    }
    if (watched == formula_bar_ && event && event->type() == QEvent::FocusOut && formula_editing_) {
        QTimer::singleShot(0, this, [this]() {
            if (!formula_editing_) return;
            QWidget* focused = QApplication::focusWidget();
            const bool focus_in_formula =
                focused && (focused == formula_bar_ || formula_bar_->isAncestorOf(focused));
            const bool focus_in_table =
                focused && (focused == table_ || focused == table_->viewport() || table_->isAncestorOf(focused));
            if (!focus_in_formula && !focus_in_table && !table_->HasInlineEditor()) {
                EndFormulaEditingMode();
                UpdateSelectionInfo();
            }
        });
    }
    return QMainWindow::eventFilter(watched, event);
}






