#include "mainwindow.h"

#include <algorithm>

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QColor>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEasingCurve>
#include <QEvent>
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
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QSizePolicy>
#include <QStatusBar>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "../core/basic.h"
#include "spreadsheetmodel.h"
#include "spreadsheetview.h"

namespace {

QString DefaultFormulaPlaceholder() {
    return QString::fromUtf8("输入数字、文本或公式，例如 =SUM(A1:B3)");
}

QStringList ChartTypeOptions() {
    return {QString::fromUtf8("折线图"), QString::fromUtf8("柱状图"), QString::fromUtf8("散点图")};
}

QString DefaultAiPrompt() {
    return QString::fromUtf8("请总结趋势、异常值、极值，并给出三条业务建议。");
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

QString FormatNumber(double value) {
    return QString::number(value, 'g', 12);
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

    auto* exit_action = file_menu->addAction(QString::fromUtf8("退出"));
    exit_action->setShortcut(QKeySequence::Quit);
    connect(exit_action, &QAction::triggered, this, &QWidget::close);

    auto* about_action = help_menu->addAction(QString::fromUtf8("关于"));
    connect(about_action, &QAction::triggered, this, [this]() {
        QMessageBox::information(
            this,
            QString::fromUtf8("关于 Excel Mowan"),
            QString::fromUtf8("Excel Mowan 是一个轻量 Qt 表格界面，支持单元格编辑、公式输入和选区分析入口。")
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
    table_->setAutoScroll(true);
    table_->setAutoScrollMargin(24);
    table_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    table_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    table_->setFrameShape(QFrame::NoFrame);

    table_->verticalHeader()->setDefaultSectionSize(34);
    table_->verticalHeader()->setMinimumWidth(56);
    table_->verticalHeader()->setHighlightSections(false);
    table_->verticalHeader()->setDefaultAlignment(Qt::AlignCenter);

    table_->horizontalHeader()->setDefaultSectionSize(126);
    table_->horizontalHeader()->setMinimumSectionSize(90);
    table_->horizontalHeader()->setStretchLastSection(false);
    table_->horizontalHeader()->setHighlightSections(false);
    table_->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    table_->horizontalHeader()->setFixedHeight(40);

    table_->setCurrentIndex(model_->index(0, 0));
    table_->setFocus();
}

void MainWindow::SetupStatusBar() {
    position_label_->setText(QString::fromUtf8("选区：A1"));
    value_type_label_->setText(QString::fromUtf8("类型：空值"));
    statusBar()->addWidget(position_label_);
    statusBar()->addPermanentWidget(value_type_label_);
    statusBar()->showMessage(QString::fromUtf8("就绪"), 2000);
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

    std::sort(indexes.begin(), indexes.end(), [](const QModelIndex& lhs, const QModelIndex& rhs) {
        if (lhs.row() != rhs.row()) return lhs.row() < rhs.row();
        return lhs.column() < rhs.column();
    });

    indexes.erase(
        std::unique(indexes.begin(), indexes.end(), [](const QModelIndex& lhs, const QModelIndex& rhs) {
            return lhs.row() == rhs.row() && lhs.column() == rhs.column();
        }),
        indexes.end()
    );

    return indexes;
}

QString MainWindow::CurrentSelectionFormulaReference() const {
    const QModelIndexList indexes = SelectedIndexes();
    if (indexes.isEmpty()) return {};

    int min_row = indexes.front().row();
    int max_row = indexes.front().row();
    int min_col = indexes.front().column();
    int max_col = indexes.front().column();

    for (const QModelIndex& index : indexes) {
        min_row = std::min(min_row, index.row());
        max_row = std::max(max_row, index.row());
        min_col = std::min(min_col, index.column());
        max_col = std::max(max_col, index.column());
    }

    return RangeReference(model_->index(min_row, min_col), model_->index(max_row, max_col));
}

AnalysisTableSnapshot MainWindow::BuildAnalysisSnapshot() const {
    AnalysisTableSnapshot snapshot;
    snapshot.sheet_name = QStringLiteral("Sheet1");

    const QModelIndexList indexes = SelectedIndexes();
    snapshot.selected_cell_count = indexes.size();
    if (indexes.isEmpty()) {
        return snapshot;
    }

    int min_row = indexes.front().row();
    int max_row = indexes.front().row();
    int min_col = indexes.front().column();
    int max_col = indexes.front().column();

    for (const QModelIndex& index : indexes) {
        min_row = std::min(min_row, index.row());
        max_row = std::max(max_row, index.row());
        min_col = std::min(min_col, index.column());
        max_col = std::max(max_col, index.column());
    }

    snapshot.range_reference = RangeReference(model_->index(min_row, min_col), model_->index(max_row, max_col));
    for (int col = min_col; col <= max_col; ++col) {
        snapshot.column_headers << model_->headerData(col, Qt::Horizontal, Qt::DisplayRole).toString();
    }

    for (int row = min_row; row <= max_row; ++row) {
        QStringList raw_row;
        QStringList display_row;
        for (int col = min_col; col <= max_col; ++col) {
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
    model_->clearAll();
    formula_bar_->clear();
    table_->clearSelection();
    table_->setCurrentIndex(model_->index(0, 0));
    EndFormulaEditingMode();
    statusBar()->showMessage(QString::fromUtf8("已新建空白表格"), 1500);
    UpdateSelectionInfo();
}

void MainWindow::UpdateAnalysisPanel() {}

void MainWindow::TriggerPlotPreview() {
    const AnalysisTableSnapshot snapshot = BuildAnalysisSnapshot();
    if (snapshot.selected_cell_count <= 0) {
        statusBar()->showMessage(QString::fromUtf8("请先选择要绘图的数据区域"), 2000);
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QString::fromUtf8("图表请求"));
    dialog.resize(680, 520);

    auto* layout = new QVBoxLayout(&dialog);
    auto* summary = new QLabel(
        QString::fromUtf8("当前选区：%1，共 %2 个单元格").arg(snapshot.range_reference).arg(snapshot.selected_cell_count),
        &dialog
    );
    summary->setWordWrap(true);

    auto* form = new QFormLayout();
    auto* chart_type_combo = new QComboBox(&dialog);
    chart_type_combo->addItems(ChartTypeOptions());
    chart_type_combo->setCurrentText(preferred_chart_type_);

    auto* title_edit = new QLineEdit(QString::fromUtf8("图表预览 - %1").arg(snapshot.range_reference), &dialog);
    form->addRow(QString::fromUtf8("图表类型"), chart_type_combo);
    form->addRow(QString::fromUtf8("标题"), title_edit);

    auto* output_edit = new QPlainTextEdit(&dialog);
    output_edit->setReadOnly(true);
    output_edit->setPlaceholderText(QString::fromUtf8("生成后会在这里显示图表请求或返回结果。"));
    output_edit->setPlainText(last_plot_result_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    auto* generate_button = buttons->addButton(QString::fromUtf8("生成请求"), QDialogButtonBox::AcceptRole);

    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(generate_button, &QPushButton::clicked, &dialog, [this, snapshot, chart_type_combo, title_edit, output_edit]() {
        preferred_chart_type_ = chart_type_combo->currentText();

        PlotRequest request;
        request.chart_type = preferred_chart_type_;
        request.title = title_edit->text().trimmed();
        if (request.title.isEmpty()) {
            request.title = QString::fromUtf8("图表预览 - %1").arg(snapshot.range_reference);
        }
        request.selection = snapshot;

        last_plot_result_ = analysis_bridge_.GeneratePlot(request);
        output_edit->setPlainText(last_plot_result_);
        statusBar()->showMessage(QString::fromUtf8("已生成图表请求"), 2000);
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
        statusBar()->showMessage(QString::fromUtf8("请先选择要分析的数据区域"), 2000);
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QString::fromUtf8("AI 分析请求"));
    dialog.resize(760, 620);

    auto* layout = new QVBoxLayout(&dialog);
    auto* summary = new QLabel(
        QString::fromUtf8("当前选区：%1，共 %2 个单元格").arg(snapshot.range_reference).arg(snapshot.selected_cell_count),
        &dialog
    );
    summary->setWordWrap(true);

    auto* form = new QFormLayout();
    auto* model_edit = new QLineEdit(ai_model_name_, &dialog);
    form->addRow(QString::fromUtf8("模型"), model_edit);

    auto* prompt_edit = new QPlainTextEdit(&dialog);
    prompt_edit->setPlaceholderText(DefaultAiPrompt());
    prompt_edit->setPlainText(ai_prompt_text_.isEmpty() ? DefaultAiPrompt() : ai_prompt_text_);

    auto* output_edit = new QPlainTextEdit(&dialog);
    output_edit->setReadOnly(true);
    output_edit->setPlaceholderText(QString::fromUtf8("生成后会在这里显示 AI 分析请求或返回结果。"));
    output_edit->setPlainText(last_ai_result_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    auto* generate_button = buttons->addButton(QString::fromUtf8("生成分析"), QDialogButtonBox::AcceptRole);

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
        statusBar()->showMessage(QString::fromUtf8("已生成 AI 分析请求"), 2000);
    });

    layout->addWidget(summary);
    layout->addLayout(form);
    layout->addWidget(new QLabel(QString::fromUtf8("分析提示词"), &dialog));
    layout->addWidget(prompt_edit);
    layout->addWidget(new QLabel(QString::fromUtf8("输出结果"), &dialog));
    layout->addWidget(output_edit, 1);
    layout->addWidget(buttons);
    dialog.exec();
}

QString MainWindow::SelectionReferenceText() const {
    const QModelIndexList indexes = SelectedIndexes();
    if (indexes.isEmpty()) return QString::fromUtf8("未选择");

    if (!table_->selectionModel()) {
        return IndexReference(indexes.front());
    }

    const QItemSelection selection = table_->selectionModel()->selection();
    if (selection.size() == 1) {
        return RangeReference(selection.front());
    }

    return QString::fromUtf8("%1 个区域，%2 个单元格").arg(selection.size()).arg(indexes.size());
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
    const int clamped = std::max(0, position);

    if (table_->HasInlineEditor()) {
        internal_inline_sync_ = true;
        table_->SetInlineEditorCursorPosition(clamped);
        internal_inline_sync_ = false;
    }

    internal_formula_update_ = true;
    formula_bar_->setCursorPosition(clamped);
    internal_formula_update_ = false;
}

void MainWindow::SetActiveFormulaSelection(int start, int length) {
    if (table_->HasInlineEditor()) {
        internal_inline_sync_ = true;
        table_->SetInlineEditorSelection(start, length);
        internal_inline_sync_ = false;
    }

    internal_formula_update_ = true;
    formula_bar_->setSelection(start, length);
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
    UpdateAnalysisPanel();

    if (indexes.isEmpty()) {
        name_box_label_->setText("--");
        selection_summary_label_->setText(QString::fromUtf8("单击单元格开始编辑，拖拽鼠标可以连续选择多个单元格。"));
        shortcut_hint_label_->setText(QString::fromUtf8("Shift 连选 | Ctrl 多选 | Delete 清空"));
        position_label_->setText(QString::fromUtf8("选区：未选择"));
        value_type_label_->setText(QString::fromUtf8("类型：空值"));
        if (sync_formula_bar) {
            formula_bar_->clear();
            formula_bar_->setPlaceholderText(DefaultFormulaPlaceholder());
        }
        return;
    }

    const QString reference = SelectionReferenceText();
    name_box_label_->setText(reference);
    position_label_->setText(QString::fromUtf8("选区：%1").arg(reference));

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
            formula_bar_->setPlaceholderText(QString::fromUtf8("将输入内容批量填充到 %1 个已选单元格").arg(indexes.size()));
        }
    }

    if (indexes.size() == 1) {
        selection_summary_label_->setText(
            QString::fromUtf8("当前单元格 %1，可直接输入内容，或在公式栏中编辑。").arg(reference)
        );
        shortcut_hint_label_->setText(
            IsFormulaEditingMode()
                ? QString::fromUtf8("公式模式：点击或拖选单元格，会替换公式中的引用")
                : QString::fromUtf8("拖拽鼠标扩展选区 | 双击或直接输入开始编辑")
        );

        if (raw_text.isEmpty()) {
            value_type_label_->setText(QString::fromUtf8("类型：空值"));
        } else if (raw_text.startsWith('=')) {
            value_type_label_->setText(QString::fromUtf8("类型：公式"));
        } else {
            bool is_number = false;
            raw_text.toDouble(&is_number);
            value_type_label_->setText(is_number ? QString::fromUtf8("类型：数字")
                                                 : QString::fromUtf8("类型：文本"));
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

        const emw::Value value = model_->grid().GetValue(emw::Address{index.row(), index.column()});
        if (value.is_number()) {
            numeric_count++;
            numeric_sum += value.number;
        } else if (value.is_text()) {
            text_count++;
        } else if (value.is_empty()) {
            empty_count++;
        } else if (value.is_error()) {
            error_count++;
        }
    }

    selection_summary_label_->setText(
        QString::fromUtf8("已选择 %1 个单元格，可批量填充内容，也可作为公式引用区域。").arg(indexes.size())
    );
    shortcut_hint_label_->setText(
        IsFormulaEditingMode()
            ? QString::fromUtf8("公式模式：拖选区域会实时写成 A1:B6 这样的引用")
            : QString::fromUtf8("Shift 连选 | Ctrl 多选 | 鼠标拖拽快速框选")
    );

    QStringList summary_parts;
    summary_parts << QString::fromUtf8("已选 %1").arg(indexes.size());
    if (numeric_count > 0) {
        summary_parts << QString::fromUtf8("数字 %1").arg(numeric_count);
        summary_parts << QString::fromUtf8("求和 %1").arg(FormatNumber(numeric_sum));
    }
    if (text_count > 0) summary_parts << QString::fromUtf8("文本 %1").arg(text_count);
    if (formula_count > 0) summary_parts << QString::fromUtf8("公式 %1").arg(formula_count);
    if (empty_count > 0) summary_parts << QString::fromUtf8("空值 %1").arg(empty_count);
    if (error_count > 0) summary_parts << QString::fromUtf8("错误 %1").arg(error_count);
    value_type_label_->setText(summary_parts.join(QString::fromUtf8(" | ")));
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
        statusBar()->showMessage(QString::fromUtf8("已更新 %1").arg(IndexReference(target)), 1500);
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
        is_batch ? QString::fromUtf8("已填充 %1 个单元格").arg(targets.size())
                 : QString::fromUtf8("已更新 %1").arg(IndexReference(targets.front())),
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

    const int cursor = std::clamp(ActiveFormulaCursorPosition(), 0, static_cast<int>(text.size()));
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

    replace_start = std::clamp(replace_start, 0, text_size);
    replace_length = std::clamp(replace_length, 0, text_size - replace_start);

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

    editing_index_ = table_->HasInlineEditor() ? table_->InlineEditIndex() : current;
    formula_editor_source_ = table_->HasInlineEditor() ? FormulaEditorSource::InlineCell
                                                       : FormulaEditorSource::FormulaBar;
    formula_editing_ = true;
    ClearFormulaReferenceTracking();

    const QString text = QString("=%1()").arg(function_name);
    if (table_->HasInlineEditor()) {
        table_->SetInlineEditorText(text);
        table_->SetInlineEditorSelection(text.size() - 1, 0);
        table_->FocusInlineEditor();
        SyncFormulaBarFromInlineEditor();
    } else {
        internal_formula_update_ = true;
        formula_bar_->setText(text);
        formula_bar_->setFocus();
        formula_bar_->setCursorPosition(text.size() - 1);
        internal_formula_update_ = false;
    }

    statusBar()->showMessage(QString::fromUtf8("已插入 %1 模板").arg(function_name), 1500);
    UpdateSelectionInfo(false);
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
    statusBar()->showMessage(QString::fromUtf8("已清空选区"), 1500);
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
