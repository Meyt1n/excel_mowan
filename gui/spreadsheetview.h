#pragma once

#include <functional>

#include <QEvent>
#include <QItemSelectionRange>
#include <QModelIndex>
#include <QTableView>

using namespace std;


class QKeyEvent;
class QLineEdit;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QWidget;

class SpreadsheetView : public QTableView {
public:
    explicit SpreadsheetView(QWidget* parent = nullptr);

    void SetInlineEditStartedCallback(function<void(const QModelIndex&, const QString&)> callback);
    void SetInlineEditTextChangedCallback(function<void(const QModelIndex&, const QString&)> callback);
    void SetInlineEditContextChangedCallback(function<void()> callback);
    void SetInlineEditFinishedCallback(function<void(const QModelIndex&, bool)> callback);
    void SetFillHandleDraggedCallback(
        function<void(const QItemSelectionRange&, const QItemSelectionRange&)> callback
    );

    bool HasInlineEditor() const;
    bool IsInlineFormulaEditing() const;
    QModelIndex InlineEditIndex() const;
    QString InlineEditorText() const;
    int InlineEditorCursorPosition() const;
    int InlineEditorSelectionStart() const;
    int InlineEditorSelectionLength() const;
    void SetInlineEditorText(const QString& text);
    void SetInlineEditorCursorPosition(int position);
    void SetInlineEditorSelection(int start, int length);
    void FocusInlineEditor();
    void CommitInlineEditor();
    void CancelInlineEditor();
    void StartInlineEdit(const QModelIndex& index, const QString& seed_text, bool replace_all);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;

private:
    bool ShouldStartTypingEdit(QKeyEvent* event) const;
    QItemSelectionRange SelectionBounds() const;
    QRect SelectionVisualRect(const QItemSelectionRange& range) const;
    QRect FillHandleRect(const QItemSelectionRange& range) const;
    bool IsPointOnFillHandle(const QPoint& pos) const;
    int ResolveRowAtPosition(int y) const;
    int ResolveColumnAtPosition(int x) const;
    void UpdateFillDragTarget(const QPoint& pos);
    void FinishFillDrag(bool apply);
    void UpdateHoverCursor(const QPoint& pos);
    void BeginInlineEdit(const QModelIndex& index, const QString& seed_text, bool replace_all);
    void FinishInlineEdit(bool commit);
    void UpdateInlineEditorGeometry();
    void NotifyInlineContextChanged();
    bool FocusStaysInsideSpreadsheet(QWidget* widget) const;

    QLineEdit* inline_editor_ = nullptr;
    QModelIndex inline_edit_index_;
    bool inline_formula_mode_ = false;
    bool suppress_inline_notifications_ = false;
    bool fill_drag_active_ = false;
    QItemSelectionRange fill_source_range_;
    QItemSelectionRange fill_target_range_;

    function<void(const QModelIndex&, const QString&)> inline_edit_started_callback_;
    function<void(const QModelIndex&, const QString&)> inline_edit_text_changed_callback_;
    function<void()> inline_edit_context_changed_callback_;
    function<void(const QModelIndex&, bool)> inline_edit_finished_callback_;
    function<void(const QItemSelectionRange&, const QItemSelectionRange&)> fill_handle_dragged_callback_;
};
