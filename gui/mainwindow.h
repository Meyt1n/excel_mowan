#pragma once

#include <QColor>
#include <QFutureWatcher>
#include <QItemSelectionRange>
#include <QMainWindow>
#include <QModelIndex>
#include <QPoint>
#include <QSet>
#include <QVector>

#include <memory>

#include "../io/dat_file.h"

class QLabel;
class QLineEdit;
class QFrame;
class QGraphicsDropShadowEffect;
class SpreadsheetView;
class SpreadsheetModel;

// 主窗口：负责菜单、公式栏、表格交互与文件/图表流程。
class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    bool eventFilter(QObject* watched, QEvent* event) override;

    void SetupWindow();
    void SetupMenuBar();
    void SetupToolbar();
    void SetupTable();
    void SetupStatusBar();
    void SetupConnections();
    void SetupCardEffects();
    void PlayIntroAnimations();
    void UpdateSelectionInfo(bool sync_formula_bar = true);
    void ApplyFormulaBarToCurrentCell();
    bool IsFormulaEditingMode() const;
    void StartFormulaEditingMode();
    void EndFormulaEditingMode();
    QModelIndex ResolveFormulaTargetIndex() const;
    bool ShouldCaptureSelectionAsFormulaReference() const;
    void CommitActiveFormulaEditToCell();
    void UpdateFormulaReferenceFromSelection();
    void ReplaceFormulaReference(const QString& reference);
    void ClearFormulaReferenceTracking();
    void InsertFunctionTemplate(const QString& function_name);
    QString ActiveFormulaText() const;
    int ActiveFormulaCursorPosition() const;
    int ActiveFormulaSelectionStart() const;
    int ActiveFormulaSelectionLength() const;
    void SetActiveFormulaText(const QString& text);
    void SetActiveFormulaCursorPosition(int position);
    void SyncFormulaBarFromInlineEditor();
    void SyncInlineEditorFromFormulaBar();
    QString SelectionReferenceText() const;
    QModelIndexList SelectedIndexes() const;
    QString CurrentSelectionFormulaReference() const;
    void ResetCurrentSheet();
    void ImportInFile();
    void OpenCsvFile();
    void OpenDatFile();
    void CompareStorageEfficiencyFiles();
    void SaveFile();
    void SaveFileAs();
    void ShowHelpGuide();
    bool SaveFileByPath(const QString& path);
    bool SaveDatFileAs(const QString& path);
    bool SaveCsvFileAs(const QString& path);
    emw::DatDocument BuildCurrentDocument() const;
    void ApplyDocument(const emw::DatDocument& document);
    void ApplyDocument(const emw::DatDocument& document, std::shared_ptr<emw::SpreadsheetGrid> prepared_grid);
    void OpenDocumentAsync(const QString& path, bool is_dat);
    void OnOpenDocumentFinished();
    void ClearSelectedCells();
    void CopySelectionToClipboard(bool cut);
    void PasteFromClipboard();
    void ShowTableContextMenu(const QPoint& pos);
    bool MergeSelectedCells();
    bool UnmergeSelectedCells();
    void ApplyAutoFill(const QItemSelectionRange& source, const QItemSelectionRange& target);
    void ResizeSelectedCells(int column_delta, int row_delta);
    void ResetSelectedCellSizes();
    void SetSelectedTextColor();
    void ClearSelectedTextColor();
    void SetSelectedFillColor();
    void ClearSelectedFillColor();
    void SortSelectedRange(bool ascending);
    void PlotSelectionWithPython(const QString& chart_type);
    QString ExportSelectionCsvForPlot(const QItemSelectionRange& range) const;
    QString ResolvePlotScriptPath() const;
    QItemSelectionRange CurrentSelectionRange() const;
    QString SelectionClipboardText() const;
    void UnmergeRangesIntersecting(const QItemSelectionRange& range);
    int FindMergedRangeContaining(const QModelIndex& index) const;

    SpreadsheetView* table_ = nullptr;
    SpreadsheetModel* model_ = nullptr;
    QLineEdit* formula_bar_ = nullptr;
    QFrame* formula_card_ = nullptr;
    QFrame* table_card_ = nullptr;
    QLabel* name_box_label_ = nullptr;
    QLabel* selection_summary_label_ = nullptr;
    QLabel* shortcut_hint_label_ = nullptr;
    QLabel* position_label_ = nullptr;
    QLabel* value_type_label_ = nullptr;
    QGraphicsDropShadowEffect* formula_shadow_ = nullptr;
    QGraphicsDropShadowEffect* table_shadow_ = nullptr;
    QString current_document_path_;
    int current_sheet_rows_ = 1;
    int current_sheet_cols_ = 1;
    QColor last_text_color_;
    QColor last_fill_color_;
    QModelIndex editing_index_;
    bool formula_editing_ = false;
    bool formula_reference_active_ = false;
    bool internal_formula_update_ = false;
    bool internal_inline_sync_ = false;
    int formula_reference_start_ = -1;
    int formula_reference_length_ = 0;
    QVector<QItemSelectionRange> merged_ranges_;
    QSet<int> resized_row_sections_;
    QSet<int> resized_col_sections_;

    struct OpenDocumentResult {
        bool ok = false;
        QString path;
        QString error;
        std::shared_ptr<emw::DatDocument> document;
        std::shared_ptr<emw::SpreadsheetGrid> prepared_grid;
    };
    QFutureWatcher<OpenDocumentResult>* open_watcher_ = nullptr;
    bool open_in_progress_ = false;

    enum class FormulaEditorSource {
        None,
        FormulaBar,
        InlineCell
    };

    FormulaEditorSource formula_editor_source_ = FormulaEditorSource::None;
};
