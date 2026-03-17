#include "spreadsheetview.h"

#include <algorithm>

#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QWidget>

#include "spreadsheetmodel.h"

SpreadsheetView::SpreadsheetView(QWidget* parent) : QTableView(parent) {
    inline_editor_ = new QLineEdit(viewport());
    inline_editor_->hide();
    inline_editor_->installEventFilter(this);
    inline_editor_->setObjectName("inlineCellEditor");
    inline_editor_->setClearButtonEnabled(false);
    inline_editor_->setAttribute(Qt::WA_MacShowFocusRect, false);
    inline_editor_->setStyleSheet(
        "QLineEdit#inlineCellEditor {"
        "  background: rgba(255, 255, 255, 0.97);"
        "  border: 2px solid #217346;"
        "  border-radius: 10px;"
        "  padding: 6px 10px;"
        "  selection-background-color: #217346;"
        "  selection-color: white;"
        "}"
    );

    connect(inline_editor_, &QLineEdit::textEdited, this, [this](const QString& text) {
        inline_formula_mode_ = text.startsWith('=');
        UpdateInlineEditorGeometry();
        if (!suppress_inline_notifications_ && inline_edit_text_changed_callback_ && inline_edit_index_.isValid()) {
            inline_edit_text_changed_callback_(inline_edit_index_, text);
        }
    });

    connect(inline_editor_, &QLineEdit::cursorPositionChanged, this, [this](int, int) {
        if (!suppress_inline_notifications_) {
            NotifyInlineContextChanged();
        }
    });

    connect(inline_editor_, &QLineEdit::selectionChanged, this, [this]() {
        if (!suppress_inline_notifications_) {
            NotifyInlineContextChanged();
        }
    });
}

void SpreadsheetView::SetInlineEditStartedCallback(
    std::function<void(const QModelIndex&, const QString&)> callback
) {
    inline_edit_started_callback_ = std::move(callback);
}

void SpreadsheetView::SetInlineEditTextChangedCallback(
    std::function<void(const QModelIndex&, const QString&)> callback
) {
    inline_edit_text_changed_callback_ = std::move(callback);
}

void SpreadsheetView::SetInlineEditContextChangedCallback(std::function<void()> callback) {
    inline_edit_context_changed_callback_ = std::move(callback);
}

void SpreadsheetView::SetInlineEditFinishedCallback(
    std::function<void(const QModelIndex&, bool)> callback
) {
    inline_edit_finished_callback_ = std::move(callback);
}

bool SpreadsheetView::HasInlineEditor() const {
    return inline_editor_ && inline_editor_->isVisible() && inline_edit_index_.isValid();
}

bool SpreadsheetView::IsInlineFormulaEditing() const {
    return HasInlineEditor() && inline_formula_mode_;
}

QModelIndex SpreadsheetView::InlineEditIndex() const {
    return inline_edit_index_;
}

QString SpreadsheetView::InlineEditorText() const {
    return HasInlineEditor() ? inline_editor_->text() : QString();
}

int SpreadsheetView::InlineEditorCursorPosition() const {
    return HasInlineEditor() ? inline_editor_->cursorPosition() : -1;
}

int SpreadsheetView::InlineEditorSelectionStart() const {
    return HasInlineEditor() ? inline_editor_->selectionStart() : -1;
}

int SpreadsheetView::InlineEditorSelectionLength() const {
    if (!HasInlineEditor()) return 0;
    const int start = inline_editor_->selectionStart();
    return start >= 0 ? inline_editor_->selectedText().size() : 0;
}

void SpreadsheetView::SetInlineEditorText(const QString& text) {
    if (!HasInlineEditor()) return;
    suppress_inline_notifications_ = true;
    inline_editor_->setText(text);
    suppress_inline_notifications_ = false;
    inline_formula_mode_ = text.startsWith('=');
    UpdateInlineEditorGeometry();
}

void SpreadsheetView::SetInlineEditorCursorPosition(int position) {
    if (!HasInlineEditor()) return;
    suppress_inline_notifications_ = true;
    inline_editor_->setCursorPosition(position);
    suppress_inline_notifications_ = false;
}

void SpreadsheetView::SetInlineEditorSelection(int start, int length) {
    if (!HasInlineEditor()) return;
    suppress_inline_notifications_ = true;
    inline_editor_->setSelection(start, length);
    suppress_inline_notifications_ = false;
}

void SpreadsheetView::FocusInlineEditor() {
    if (!HasInlineEditor()) return;
    inline_editor_->show();
    inline_editor_->raise();
    inline_editor_->setFocus();
}

void SpreadsheetView::CommitInlineEditor() {
    FinishInlineEdit(true);
}

void SpreadsheetView::CancelInlineEditor() {
    FinishInlineEdit(false);
}

bool SpreadsheetView::eventFilter(QObject* watched, QEvent* event) {
    if (watched == inline_editor_ && event) {
        if (event->type() == QEvent::KeyPress) {
            auto* key_event = static_cast<QKeyEvent*>(event);
            if (key_event->key() == Qt::Key_Return || key_event->key() == Qt::Key_Enter) {
                FinishInlineEdit(true);
                return true;
            }
            if (key_event->key() == Qt::Key_Escape) {
                FinishInlineEdit(false);
                return true;
            }
        }
        if (event->type() == QEvent::FocusOut && HasInlineEditor()) {
            QWidget* focused = QApplication::focusWidget();
            if (!FocusStaysInsideSpreadsheet(focused)) {
                FinishInlineEdit(true);
            }
        }
    }
    return QTableView::eventFilter(watched, event);
}

bool SpreadsheetView::ShouldStartTypingEdit(QKeyEvent* event) const {
    if (!event) return false;
    if (!currentIndex().isValid()) return false;

    Qt::KeyboardModifiers mods = event->modifiers();
    if ((mods & Qt::ControlModifier) || (mods & Qt::AltModifier) || (mods & Qt::MetaModifier)) {
        return false;
    }

    const QString text = event->text();
    if (text.isEmpty()) return false;
    const QChar ch = text.at(0);
    return ch.isPrint() && !ch.isNull();
}

void SpreadsheetView::keyPressEvent(QKeyEvent* event) {
    if (!event) {
        QTableView::keyPressEvent(event);
        return;
    }

    if (HasInlineEditor()) {
        if (event->key() == Qt::Key_F2) {
            FocusInlineEditor();
            return;
        }
        if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
            if (!inline_editor_->hasFocus()) {
                SetInlineEditorText(QString());
                inline_editor_->setCursorPosition(0);
                return;
            }
        }
    }

    if (!HasInlineEditor() && (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace)) {
        auto* spreadsheet_model = dynamic_cast<SpreadsheetModel*>(model());
        if (spreadsheet_model && selectionModel()) {
            QModelIndexList indexes = selectionModel()->selectedIndexes();
            if (indexes.isEmpty() && currentIndex().isValid()) {
                indexes.push_back(currentIndex());
            }
            if (!indexes.isEmpty() && spreadsheet_model->setData(indexes, QString(), Qt::EditRole)) {
                return;
            }
        }
    }

    if (event->key() == Qt::Key_F2 && currentIndex().isValid()) {
        setFocus(Qt::OtherFocusReason);
        BeginInlineEdit(currentIndex(), model()->data(currentIndex(), Qt::EditRole).toString(), false);
        return;
    }

    if (ShouldStartTypingEdit(event)) {
        setFocus(Qt::OtherFocusReason);
        BeginInlineEdit(currentIndex(), event->text(), true);
        return;
    }

    if (HasInlineEditor() && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
        FinishInlineEdit(true);
        return;
    }

    QTableView::keyPressEvent(event);
}

void SpreadsheetView::mousePressEvent(QMouseEvent* event) {
    if (!event) {
        QTableView::mousePressEvent(event);
        return;
    }

    const QModelIndex clicked = indexAt(event->pos());
    if (HasInlineEditor() && !inline_formula_mode_ && clicked != inline_edit_index_) {
        FinishInlineEdit(true);
    }

    QTableView::mousePressEvent(event);
    if (clicked.isValid()) {
        setCurrentIndex(clicked);
    }
    setFocus(Qt::MouseFocusReason);
}

void SpreadsheetView::mouseDoubleClickEvent(QMouseEvent* event) {
    if (!event) {
        QTableView::mouseDoubleClickEvent(event);
        return;
    }

    const QModelIndex clicked = indexAt(event->pos());
    if (clicked.isValid()) {
        setCurrentIndex(clicked);
        BeginInlineEdit(clicked, model()->data(clicked, Qt::EditRole).toString(), false);
        return;
    }
    QTableView::mouseDoubleClickEvent(event);
}

void SpreadsheetView::mouseMoveEvent(QMouseEvent* event) {
    if (event) {
        UpdateHoverCursor(event->pos());
    }
    QTableView::mouseMoveEvent(event);
}

void SpreadsheetView::leaveEvent(QEvent* event) {
    viewport()->unsetCursor();
    QTableView::leaveEvent(event);
}

void SpreadsheetView::resizeEvent(QResizeEvent* event) {
    QTableView::resizeEvent(event);
    UpdateInlineEditorGeometry();
}

void SpreadsheetView::scrollContentsBy(int dx, int dy) {
    QTableView::scrollContentsBy(dx, dy);
    UpdateInlineEditorGeometry();
}

void SpreadsheetView::UpdateHoverCursor(const QPoint& pos) {
    if (indexAt(pos).isValid()) {
        viewport()->setCursor(Qt::CrossCursor);
        return;
    }
    viewport()->unsetCursor();
}

void SpreadsheetView::BeginInlineEdit(const QModelIndex& index, const QString& seed_text, bool replace_all) {
    if (!index.isValid() || !model()) return;

    if (HasInlineEditor() && inline_edit_index_.isValid() && inline_edit_index_ != index) {
        FinishInlineEdit(true);
    }

    inline_edit_index_ = index;
    setCurrentIndex(index);

    const QString initial_text = replace_all ? seed_text : model()->data(index, Qt::EditRole).toString();

    suppress_inline_notifications_ = true;
    inline_editor_->setText(initial_text);
    if (replace_all) {
        inline_editor_->setCursorPosition(initial_text.size());
    } else {
        inline_editor_->selectAll();
    }
    suppress_inline_notifications_ = false;

    inline_formula_mode_ = initial_text.startsWith('=');
    inline_editor_->show();
    UpdateInlineEditorGeometry();
    inline_editor_->raise();
    inline_editor_->setFocus();
    QTimer::singleShot(0, this, [this]() {
        if (inline_edit_index_.isValid()) {
            UpdateInlineEditorGeometry();
        }
    });

    if (inline_edit_started_callback_) {
        inline_edit_started_callback_(inline_edit_index_, initial_text);
    }
    NotifyInlineContextChanged();
}

void SpreadsheetView::FinishInlineEdit(bool commit) {
    if (!HasInlineEditor()) return;

    const QModelIndex index = inline_edit_index_;
    const QString text = inline_editor_->text();

    inline_editor_->hide();
    inline_edit_index_ = QModelIndex();
    inline_formula_mode_ = false;

    if (commit && model() && index.isValid()) {
        model()->setData(index, text, Qt::EditRole);
        setCurrentIndex(index);
    }

    if (inline_edit_finished_callback_ && index.isValid()) {
        inline_edit_finished_callback_(index, commit);
    }

    setFocus(Qt::OtherFocusReason);
}

void SpreadsheetView::UpdateInlineEditorGeometry() {
    if (!inline_editor_ || !inline_edit_index_.isValid()) return;

    QRect rect = visualRect(inline_edit_index_);
    if (!rect.isValid()) {
        inline_editor_->hide();
        return;
    }

    const int preferred_width =
        std::max(rect.width() + 40, 28 + inline_editor_->fontMetrics().horizontalAdvance(inline_editor_->text() + "  "));
    const int width = std::min(viewport()->width() - rect.x() - 2, preferred_width + 100);
    QRect editor_rect = rect.adjusted(-1, -1, std::max(12, width - rect.width()), 1);
    editor_rect.setWidth(std::max(rect.width(), editor_rect.width()));
    editor_rect.setHeight(rect.height() + 2);

    inline_editor_->setGeometry(editor_rect);
    inline_editor_->raise();
}

void SpreadsheetView::NotifyInlineContextChanged() {
    if (inline_edit_context_changed_callback_) {
        inline_edit_context_changed_callback_();
    }
}

bool SpreadsheetView::FocusStaysInsideSpreadsheet(QWidget* widget) const {
    if (!widget) return false;
    return widget == this || widget == viewport() || isAncestorOf(widget) || viewport()->isAncestorOf(widget);
}
