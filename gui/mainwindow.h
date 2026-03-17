#pragma once

#include "analysisbridge.h"

#include <QMainWindow>
#include <QModelIndex>

class QLabel;
class QLineEdit;
class QFrame;
class QGraphicsDropShadowEffect;
class SpreadsheetView;
class SpreadsheetModel;

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);
    AnalysisBridge& analysisBridge() { return analysis_bridge_; }
    const AnalysisBridge& analysisBridge() const { return analysis_bridge_; }

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
    void SetActiveFormulaSelection(int start, int length);
    void SyncFormulaBarFromInlineEditor();
    void SyncInlineEditorFromFormulaBar();
    QString SelectionReferenceText() const;
    QModelIndexList SelectedIndexes() const;
    QString CurrentSelectionFormulaReference() const;
    AnalysisTableSnapshot BuildAnalysisSnapshot() const;
    void ResetCurrentSheet();
    void UpdateAnalysisPanel();
    void TriggerPlotPreview();
    void TriggerAiAnalysis();
    void ClearSelectedCells();

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
    AnalysisBridge analysis_bridge_;
    QString preferred_chart_type_ = QString::fromUtf8("折线图");
    QString ai_model_name_ = QStringLiteral("qwen2.5:7b");
    QString ai_prompt_text_;
    QString last_plot_result_;
    QString last_ai_result_;
    QModelIndex editing_index_;
    bool formula_editing_ = false;
    bool formula_reference_active_ = false;
    bool internal_formula_update_ = false;
    bool internal_inline_sync_ = false;
    int formula_reference_start_ = -1;
    int formula_reference_length_ = 0;

    enum class FormulaEditorSource {
        None,
        FormulaBar,
        InlineCell
    };

    FormulaEditorSource formula_editor_source_ = FormulaEditorSource::None;
};
