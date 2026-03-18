#include "mainwindow.h"

#include <algorithm>

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEasingCurve>
#include <QEvent>
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
#include <QSizePolicy>
#include <QStatusBar>
#include <QStringList>
#include <QTimer>
#include <QVector>
#include <QVBoxLayout>
#include <QWidget>
#include <cmath>
#include <set>
#include <unordered_map>

#include "../app/table_file_io.h"
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

QStringList ChartTypeOptions() {
    return {
        QStringLiteral("\u6298\u7ebf\u56fe"),
        QStringLiteral("\u67f1\u72b6\u56fe"),
        QStringLiteral("\u6563\u70b9\u56fe")
    };
}

QString DefaultAiPrompt() {
    return QStringLiteral(
        "\u8bf7\u6839\u636e\u9009\u533a\u4e2d\u7684\u8868\u683c\u5185\u5bb9\uff0c\u7528\u7b80\u6d01\u7684\u4e2d\u6587\u603b\u7ed3\u5173\u952e"
        "\u8d8b\u52bf\u3001\u5f02\u5e38\u70b9\u548c\u53ef\u6267\u884c\u5efa\u8bae\u3002"
    );
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
    out.bold = style.bold;
    out.italic = style.italic;
    out.has_alignment = style.has_alignment;
    out.alignment = style.has_alignment ? static_cast<uint32_t>(style.alignment) : 0;
    return out;
}

SpreadsheetModel::CellStyle ToModelStyle(const emw::DatCellStyle& style) {
    SpreadsheetModel::CellStyle out;
    out.has_foreground = style.has_foreground;
    out.foreground = style.has_foreground ? DecodeColorRgba(style.foreground_rgba) : QColor();
    out.has_background = style.has_background;
    out.background = style.has_background ? DecodeColorRgba(style.background_rgba) : QColor();
    out.bold = style.bold;
    out.italic = style.italic;
    out.has_alignment = style.has_alignment;
    out.alignment = style.has_alignment ? static_cast<Qt::Alignment>(style.alignment)
                                        : static_cast<Qt::Alignment>(Qt::AlignLeft | Qt::AlignVCenter);
    return out;
}

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
    auto* table_menu = menuBar()->addMenu(QString::fromUtf8("建表(&T)"));
    auto* edit_menu = menuBar()->addMenu(QString::fromUtf8("编辑(&E)"));
    auto* formula_menu = menuBar()->addMenu(QString::fromUtf8("公式(&M)"));
    auto* analysis_menu = menuBar()->addMenu(QString::fromUtf8("AI/分析(&A)"));
    auto* file_menu = menuBar()->addMenu(QString::fromUtf8("文件(&F)"));
    auto* help_menu = menuBar()->addMenu(QString::fromUtf8("帮助(&H)"));

    auto* new_sheet_action = table_menu->addAction(QString::fromUtf8("新建空白表格"));
    new_sheet_action->setShortcut(QKeySequence::New);
    connect(new_sheet_action, &QAction::triggered, this, &MainWindow::ResetCurrentSheet);

    auto* clear_selection_action = edit_menu->addAction(QString::fromUtf8("清空选区"));
    clear_selection_action->setShortcut(QKeySequence::Delete);
    connect(clear_selection_action, &QAction::triggered, this, &MainWindow::ClearSelectedCells);

    auto* cut_action = edit_menu->addAction(QString::fromUtf8("剪切"));
    cut_action->setShortcut(QKeySequence::Cut);
    connect(cut_action, &QAction::triggered, this, [this]() { CopySelectionToClipboard(true); });

    auto* copy_action = edit_menu->addAction(QString::fromUtf8("复制"));
    copy_action->setShortcut(QKeySequence::Copy);
    connect(copy_action, &QAction::triggered, this, [this]() { CopySelectionToClipboard(false); });

    auto* paste_action = edit_menu->addAction(QString::fromUtf8("粘贴"));
    paste_action->setShortcut(QKeySequence::Paste);
    connect(paste_action, &QAction::triggered, this, &MainWindow::PasteFromClipboard);

    edit_menu->addSeparator();

    auto* merge_cells_action = edit_menu->addAction(QString::fromUtf8("合并单元格"));
    merge_cells_action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
    connect(merge_cells_action, &QAction::triggered, this, &MainWindow::MergeSelectedCells);

    auto* unmerge_cells_action = edit_menu->addAction(QString::fromUtf8("取消合并单元格"));
    unmerge_cells_action->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M));
    connect(unmerge_cells_action, &QAction::triggered, this, &MainWindow::UnmergeSelectedCells);

    auto* enlarge_cells_action = edit_menu->addAction(QString::fromUtf8("增大单元格"));
    enlarge_cells_action->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Plus));
    connect(enlarge_cells_action, &QAction::triggered, this, [this]() {
        ResizeSelectedCells(kCellResizeStep, kCellResizeStep / 2);
    });

    auto* shrink_cells_action = edit_menu->addAction(QString::fromUtf8("缩小单元格"));
    shrink_cells_action->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Underscore));
    connect(shrink_cells_action, &QAction::triggered, this, [this]() {
        ResizeSelectedCells(-kCellResizeStep, -(kCellResizeStep / 2));
    });

    auto* reset_cell_size_action = edit_menu->addAction(QString::fromUtf8("恢复默认单元格大小"));
    reset_cell_size_action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(reset_cell_size_action, &QAction::triggered, this, &MainWindow::ResetSelectedCellSizes);

    auto* recalc_action = edit_menu->addAction(QString::fromUtf8("重新计算"));
    recalc_action->setShortcut(Qt::Key_F9);
    connect(recalc_action, &QAction::triggered, this, [this]() {
        model_->recalcAll();
        statusBar()->showMessage(QString::fromUtf8("已重新计算全部公式"), 1500);
        UpdateSelectionInfo(!IsFormulaEditingMode());
    });

    formula_menu->addAction(QString::fromUtf8("插入 SUM"), this, [this]() { InsertFunctionTemplate("SUM"); });
    formula_menu->addAction(QString::fromUtf8("插入 AVG"), this, [this]() { InsertFunctionTemplate("AVG"); });
    formula_menu->addAction(QString::fromUtf8("插入 MAX"), this, [this]() { InsertFunctionTemplate("MAX"); });
    formula_menu->addAction(QString::fromUtf8("插入 MIN"), this, [this]() { InsertFunctionTemplate("MIN"); });

    analysis_menu->addAction(QString::fromUtf8("生成图表请求"), this, [this]() { TriggerPlotPreview(); });
    analysis_menu->addAction(QString::fromUtf8("生成 AI 分析请求"), this, [this]() { TriggerAiAnalysis(); });

    auto* import_in_action = file_menu->addAction(QString::fromUtf8("导入测试用例(.in)..."));
    import_in_action->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));
    connect(import_in_action, &QAction::triggered, this, &MainWindow::ImportInFile);

    auto* open_dat_action = file_menu->addAction(QString::fromUtf8("打开 DAT..."));
    open_dat_action->setShortcut(QKeySequence::Open);
    connect(open_dat_action, &QAction::triggered, this, &MainWindow::OpenDatFile);

    auto* save_dat_action = file_menu->addAction(QString::fromUtf8("保存 DAT..."));
    save_dat_action->setShortcut(QKeySequence::Save);
    connect(save_dat_action, &QAction::triggered, this, &MainWindow::SaveDatFile);

    file_menu->addSeparator();

    auto* exit_action = file_menu->addAction(QString::fromUtf8("退出"));
    exit_action->setShortcut(QKeySequence::Quit);
    connect(exit_action, &QAction::triggered, this, &QWidget::close);

    auto* about_action = help_menu->addAction(QString::fromUtf8("关于"));
    connect(about_action, &QAction::triggered, this, [this]() {
        QMessageBox::information(
            this,
            QString::fromUtf8("关于 Excel Mowan"),
            QString::fromUtf8("Excel Mowan 是一个轻量 Qt 表格界面，支持单元格编辑、公式输入、右键菜单和选区分析。")
        );
    });
}

void MainWindow::SetupToolbar() {
    formula_bar_->clear();
}

void MainWindow::SetupTable() {
    table_->setModel(model_);
    table_->setAlternatingRowColors(true);
    table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_->setSelectionBehavior(QAbstractItemView::SelectItems);
    table_->setDragDropMode(QAbstractItemView::NoDragDrop);
    table_->setDragEnabled(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setShowGrid(true);
    table_->setWordWrap(false);
    table_->setCornerButtonEnabled(true);
    table_->setSortingEnabled(false);
    table_->setTabKeyNavigation(true);
    table_->setMouseTracking(true);
    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    table_->setAutoScroll(true);
    table_->setAutoScrollMargin(24);
    table_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    table_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
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

    connect(table_, &QWidget::customContextMenuRequested, this, &MainWindow::ShowTableContextMenu);

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
    menu.addSeparator();
    auto* sum_action = menu.addAction(QStringLiteral("\u63d2\u5165 SUM"));
    auto* avg_action = menu.addAction(QStringLiteral("\u63d2\u5165 AVG"));

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
    } else if (chosen == sum_action) {
        InsertFunctionTemplate("SUM");
    } else if (chosen == avg_action) {
        InsertFunctionTemplate("AVG");
    }
}

QString MainWindow::CurrentSelectionFormulaReference() const {
    const QModelIndexList indexes = SelectedIndexes();
    if (indexes.isEmpty()) return {};

    SelectionBounds bounds;
    if (!TryGetSelectionBounds(indexes, &bounds)) return {};
    return RangeReference(model_->index(bounds.min_row, bounds.min_col), model_->index(bounds.max_row, bounds.max_col));
}

AnalysisTableSnapshot MainWindow::BuildAnalysisSnapshot() const {
    AnalysisTableSnapshot snapshot;
    snapshot.sheet_name = QStringLiteral("Sheet1");

    const QModelIndexList indexes = SelectedIndexes();
    snapshot.selected_cell_count = indexes.size();
    if (indexes.isEmpty()) {
        return snapshot;
    }

    SelectionBounds bounds;
    if (!TryGetSelectionBounds(indexes, &bounds)) return snapshot;

    snapshot.range_reference =
        RangeReference(model_->index(bounds.min_row, bounds.min_col), model_->index(bounds.max_row, bounds.max_col));
    for (int col = bounds.min_col; col <= bounds.max_col; ++col) {
        snapshot.column_headers << model_->headerData(col, Qt::Horizontal, Qt::DisplayRole).toString();
    }

    for (int row = bounds.min_row; row <= bounds.max_row; ++row) {
        QStringList raw_row;
        QStringList display_row;
        for (int col = bounds.min_col; col <= bounds.max_col; ++col) {
            const QModelIndex cell = model_->index(row, col);
            raw_row << model_->data(cell, Qt::EditRole).toString();
            display_row << model_->data(cell, Qt::DisplayRole).toString();
        }
        snapshot.raw_values.push_back(raw_row);
        snapshot.display_values.push_back(display_row);
    }

    return snapshot;
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
        QString::fromUtf8("导入测试用例"),
        current_document_path_,
        QString::fromUtf8("Input Files (*.in);;All Files (*.*)")
    );
    if (path.isEmpty()) return;

    emw::SpreadsheetGrid grid;
    int rows = 0;
    int cols = 0;
    if (!emw_app::LoadInToGrid(path.toStdString(), grid, rows, cols)) {
        QMessageBox::warning(
            this,
            QString::fromUtf8("导入失败"),
            QString::fromUtf8("无法读取该 .in 文件，请检查首行行列数和后续逗号分隔内容。")
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
        if (cell.is_formula()) {
            const emw::EvaluatedCell evaluated = grid.GetEvaluatedCell(addr);
            record.has_cached_display = true;
            record.has_error = evaluated.has_error;
            record.cached_display = evaluated.has_error ? string("#NA") : evaluated.value.to_string();
        }
        document.cells.push_back(::move(record));
    });

    ApplyDocument(document);
    current_document_path_.clear();
    statusBar()->showMessage(
        QString::fromUtf8("已导入测试用例：%1").arg(QFileInfo(path).fileName()),
        2000
    );
}

void MainWindow::OpenDatFile() {
    const QString path = QFileDialog::getOpenFileName(
        this,
        QString::fromUtf8("打开 DAT 文件"),
        current_document_path_,
        QString::fromUtf8("DAT Files (*.dat);;All Files (*.*)")
    );
    if (path.isEmpty()) return;

    emw::DatDocument document;
    string error;
    if (!emw::DatFile::LoadDocument(path.toStdString(), document, &error)) {
        QMessageBox::warning(
            this,
            QString::fromUtf8("打开失败"),
            QString::fromUtf8("无法读取该 DAT 文件：%1").arg(QString::fromStdString(error))
        );
        return;
    }

    ApplyDocument(document);
    current_document_path_ = path;
    statusBar()->showMessage(
        QString::fromUtf8("已打开：%1").arg(QFileInfo(path).fileName()),
        2000
    );
}

void MainWindow::SaveDatFile() {
    QString path = current_document_path_;
    if (path.isEmpty()) {
        path = QFileDialog::getSaveFileName(
            this,
            QString::fromUtf8("保存 DAT 文件"),
            QStringLiteral("sheet.dat"),
            QString::fromUtf8("DAT Files (*.dat);;All Files (*.*)")
        );
        if (path.isEmpty()) return;
    }

    if (QFileInfo(path).suffix().isEmpty()) {
        path += QStringLiteral(".dat");
    }
    SaveDatFileAs(path);
}

bool MainWindow::SaveDatFileAs(const QString& path) {
    if (table_->HasInlineEditor()) {
        table_->CommitInlineEditor();
    }

    const emw::DatDocument document = BuildCurrentDocument();
    if (!emw::DatFile::SaveDocument(path.toStdString(), document)) {
        QMessageBox::warning(
            this,
            QString::fromUtf8("保存失败"),
            QString::fromUtf8("无法写入 DAT 文件。")
        );
        return false;
    }

    current_document_path_ = path;
    statusBar()->showMessage(
        QString::fromUtf8("已保存：%1").arg(QFileInfo(path).fileName()),
        2000
    );
    return true;
}

emw::DatDocument MainWindow::BuildCurrentDocument() const {
    emw::DatDocument document;
    document.rows = max(1, current_sheet_rows_);
    document.cols = max(1, current_sheet_cols_);

    // Serialize as a sparse document: store only populated cells or cells with
    // non-default presentation metadata, plus the resized rows/columns.
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
        if (cell.is_formula()) {
            const emw::EvaluatedCell evaluated = grid.GetEvaluatedCell(addr);
            record.has_cached_display = true;
            record.has_error = evaluated.has_error;
            record.cached_display = evaluated.has_error ? string("#NA") : evaluated.value.to_string();
        }

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

void MainWindow::ApplyDocument(const emw::DatDocument& document) {
    if (table_->HasInlineEditor()) {
        table_->CommitInlineEditor();
    }

    emw::SpreadsheetGrid grid;
    for (const emw::DatCellRecord& record : document.cells) {
        if (!record.addr.is_valid()) continue;
        grid.SetCell(record.addr, record.raw, nullptr);
    }
    grid.RecalcAll();

    table_->clearSpans();
    merged_ranges_.clear();
    model_->loadFromGrid(::move(grid));

    current_sheet_rows_ = max(1, document.rows);
    current_sheet_cols_ = max(1, document.cols);

    auto* horizontal = table_->horizontalHeader();
    auto* vertical = table_->verticalHeader();
    for (int col = 0; col < model_->columnCount(); ++col) {
        horizontal->resizeSection(col, kDefaultColumnWidth);
    }
    for (int row = 0; row < model_->rowCount(); ++row) {
        vertical->resizeSection(row, kDefaultRowHeight);
    }

    for (const emw::DatCellRecord& record : document.cells) {
        if (record.style.IsDefault()) continue;
        if (!record.addr.is_valid()) continue;
        model_->setCellStyle(model_->index(record.addr.row, record.addr.col), ToModelStyle(record.style));
    }

    // Restore view-only state after cell data is in place so spans and sizes
    // map onto the current model indexes correctly.
    for (const emw::DatSizedSection& section : document.col_widths) {
        if (section.index < 0 || section.index >= model_->columnCount()) continue;
        horizontal->resizeSection(section.index, section.size);
    }
    for (const emw::DatSizedSection& section : document.row_heights) {
        if (section.index < 0 || section.index >= model_->rowCount()) continue;
        vertical->resizeSection(section.index, section.size);
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

void MainWindow::TriggerPlotPreview() {
    const AnalysisTableSnapshot snapshot = BuildAnalysisSnapshot();
    if (snapshot.selected_cell_count <= 0) {
        statusBar()->showMessage(QStringLiteral("\u8bf7\u5148\u9009\u62e9\u8981\u751f\u6210\u56fe\u8868\u7684\u6570\u636e\u533a\u57df"), 2000);
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("\u56fe\u8868\u8bf7\u6c42"));
    dialog.resize(680, 520);

    auto* layout = new QVBoxLayout(&dialog);
    auto* summary = new QLabel(
        QStringLiteral("\u5f53\u524d\u9009\u533a\uff1a%1\uff0c\u5171 %2 \u4e2a\u5355\u5143\u683c")
            .arg(snapshot.range_reference)
            .arg(snapshot.selected_cell_count),
        &dialog
    );
    summary->setWordWrap(true);

    auto* form = new QFormLayout();
    auto* chart_type_combo = new QComboBox(&dialog);
    chart_type_combo->addItems(ChartTypeOptions());
    chart_type_combo->setCurrentText(preferred_chart_type_);

    auto* title_edit = new QLineEdit(
        QStringLiteral("\u56fe\u8868\u9884\u89c8 - %1").arg(snapshot.range_reference),
        &dialog
    );
    form->addRow(QStringLiteral("\u56fe\u8868\u7c7b\u578b"), chart_type_combo);
    form->addRow(QStringLiteral("\u6807\u9898"), title_edit);

    auto* output_edit = new QPlainTextEdit(&dialog);
    output_edit->setReadOnly(true);
    output_edit->setPlaceholderText(
        QStringLiteral("\u751f\u6210\u540e\u4f1a\u5728\u8fd9\u91cc\u663e\u793a\u56fe\u8868\u8bf7\u6c42\u6216\u8fd4\u56de\u7ed3\u679c\u3002")
    );
    output_edit->setPlainText(last_plot_result_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    auto* generate_button = buttons->addButton(QStringLiteral("\u751f\u6210\u8bf7\u6c42"), QDialogButtonBox::AcceptRole);

    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(generate_button, &QPushButton::clicked, &dialog, [this, snapshot, chart_type_combo, title_edit, output_edit]() {
        preferred_chart_type_ = chart_type_combo->currentText();

        PlotRequest request;
        request.chart_type = preferred_chart_type_;
        request.title = title_edit->text().trimmed();
        if (request.title.isEmpty()) {
            request.title = QStringLiteral("\u56fe\u8868\u9884\u89c8 - %1").arg(snapshot.range_reference);
        }
        request.selection = snapshot;

        last_plot_result_ = analysis_bridge_.GeneratePlot(request);
        output_edit->setPlainText(last_plot_result_);
        statusBar()->showMessage(QStringLiteral("\u5df2\u751f\u6210\u56fe\u8868\u8bf7\u6c42"), 2000);
    });

    layout->addWidget(summary);
    layout->addLayout(form);
    layout->addWidget(output_edit, 1);
    layout->addWidget(buttons);
    dialog.exec();
}

void MainWindow::TriggerAiAnalysis() {
    const AnalysisTableSnapshot snapshot = BuildAnalysisSnapshot();
    if (snapshot.selected_cell_count <= 0) {
        statusBar()->showMessage(QStringLiteral("\u8bf7\u5148\u9009\u62e9\u8981\u5206\u6790\u7684\u6570\u636e\u533a\u57df"), 2000);
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("AI \u5206\u6790\u8bf7\u6c42"));
    dialog.resize(760, 620);

    auto* layout = new QVBoxLayout(&dialog);
    auto* summary = new QLabel(
        QStringLiteral("\u5f53\u524d\u9009\u533a\uff1a%1\uff0c\u5171 %2 \u4e2a\u5355\u5143\u683c")
            .arg(snapshot.range_reference)
            .arg(snapshot.selected_cell_count),
        &dialog
    );
    summary->setWordWrap(true);

    auto* form = new QFormLayout();
    auto* model_edit = new QLineEdit(ai_model_name_, &dialog);
    form->addRow(QStringLiteral("\u6a21\u578b"), model_edit);

    auto* prompt_edit = new QPlainTextEdit(&dialog);
    prompt_edit->setPlaceholderText(DefaultAiPrompt());
    prompt_edit->setPlainText(ai_prompt_text_.isEmpty() ? DefaultAiPrompt() : ai_prompt_text_);

    auto* output_edit = new QPlainTextEdit(&dialog);
    output_edit->setReadOnly(true);
    output_edit->setPlaceholderText(
        QStringLiteral("\u751f\u6210\u540e\u4f1a\u5728\u8fd9\u91cc\u663e\u793a AI \u5206\u6790\u8bf7\u6c42\u6216\u8fd4\u56de\u7ed3\u679c\u3002")
    );
    output_edit->setPlainText(last_ai_result_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    auto* generate_button = buttons->addButton(QStringLiteral("\u751f\u6210\u5206\u6790"), QDialogButtonBox::AcceptRole);

    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(generate_button, &QPushButton::clicked, &dialog, [this, snapshot, model_edit, prompt_edit, output_edit]() {
        ai_model_name_ = model_edit->text().trimmed();
        ai_prompt_text_ = prompt_edit->toPlainText().trimmed();

        AiAnalysisRequest request;
        request.model_name = ai_model_name_;
        request.prompt = ai_prompt_text_.isEmpty() ? DefaultAiPrompt() : ai_prompt_text_;
        request.selection = snapshot;

        last_ai_result_ = analysis_bridge_.RunAiAnalysis(request);
        output_edit->setPlainText(last_ai_result_);
        statusBar()->showMessage(QStringLiteral("\u5df2\u751f\u6210 AI \u5206\u6790\u8bf7\u6c42"), 2000);
    });

    layout->addWidget(summary);
    layout->addLayout(form);
    layout->addWidget(new QLabel(QStringLiteral("\u5206\u6790\u63d0\u793a\u8bcd"), &dialog));
    layout->addWidget(prompt_edit);
    layout->addWidget(new QLabel(QStringLiteral("\u8f93\u51fa\u7ed3\u679c"), &dialog));
    layout->addWidget(output_edit, 1);
    layout->addWidget(buttons);
    dialog.exec();
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

