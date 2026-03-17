#pragma once

#include <functional>

#include <QEvent>
#include <QModelIndex>
#include <QTableView>

class QKeyEvent;
class QLineEdit;
class QMouseEvent;
class QWidget;

class SpreadsheetView : public QTableView {
public:
    explicit SpreadsheetView(QWidget* parent = nullptr);

    void SetInlineEditStartedCallback(std::function<void(const QModelIndex&, const QString&)> callback);
    void SetInlineEditTextChangedCallback(std::function<void(const QModelIndex&, const QString&)> callback);
    void SetInlineEditContextChangedCallback(std::function<void()> callback);
    void SetInlineEditFinishedCallback(std::function<void(const QModelIndex&, bool)> callback);

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

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;

private:
    bool ShouldStartTypingEdit(QKeyEvent* event) const;
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

    std::function<void(const QModelIndex&, const QString&)> inline_edit_started_callback_;
    std::function<void(const QModelIndex&, const QString&)> inline_edit_text_changed_callback_;
    std::function<void()> inline_edit_context_changed_callback_;
    std::function<void(const QModelIndex&, bool)> inline_edit_finished_callback_;
};
