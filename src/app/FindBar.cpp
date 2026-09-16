#include "FindBar.h"

#include <QKeyEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextBrowser>
#include <QTextDocument>

namespace YuzNote {
namespace {

constexpr int kMaxMatches = 2000;
const QColor kMatchBack(255, 235, 130);
const QColor kMatchFront(0, 0, 0);
const QColor kCurrentBack(255, 160, 40);

} // namespace

FindBar::FindBar(QTextBrowser *view, QWidget *parent)
    : QWidget(parent)
    , view_(view)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    edit_ = new QLineEdit(this);
    edit_->setPlaceholderText(tr("Find in document"));
    edit_->setClearButtonEnabled(true);
    count_ = new QLabel(this);
    count_->setMinimumWidth(72);
    auto *prev = new QPushButton(tr("Prev"), this);
    auto *next = new QPushButton(tr("Next"), this);
    layout->addWidget(edit_, 1);
    layout->addWidget(count_);
    layout->addWidget(prev);
    layout->addWidget(next);
    connect(edit_, &QLineEdit::textChanged, this, &FindBar::onTextChanged);
    connect(edit_, &QLineEdit::returnPressed, this, &FindBar::findNext);
    connect(prev, &QPushButton::clicked, this, &FindBar::findPrevious);
    connect(next, &QPushButton::clicked, this, &FindBar::findNext);
    hide();
}

void FindBar::activate()
{
    show();
    edit_->setFocus(Qt::ShortcutFocusReason);
    edit_->selectAll();
}

void FindBar::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        clearAll();
        hide();
        emit closed();
        if (view_ != nullptr) {
            view_->setFocus(Qt::OtherFocusReason);
        }
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void FindBar::onTextChanged(const QString &)
{
    rebuild();
    if (current_ >= 0) {
        showCurrent();
    }
}

void FindBar::findNext()
{
    if (matches_.isEmpty()) {
        rebuild();
    }
    if (matches_.isEmpty()) {
        return;
    }
    current_ = (current_ + 1) % matches_.size();
    showCurrent();
}

void FindBar::findPrevious()
{
    if (matches_.isEmpty()) {
        rebuild();
    }
    if (matches_.isEmpty()) {
        return;
    }
    current_ = (current_ - 1 + matches_.size()) % matches_.size();
    showCurrent();
}

void FindBar::rebuild()
{
    matches_.clear();
    current_ = -1;
    count_->clear();
    if (view_ == nullptr) {
        return;
    }
    const QString text = edit_->text();
    if (text.isEmpty()) {
        view_->setExtraSelections({});
        return;
    }
    QTextDocument *doc = view_->document();
    QTextCursor from(doc);
    from.setPosition(0);
    int guard = 0;
    for (;;) {
        const QTextCursor found =
            doc->find(text, from, QTextDocument::FindFlags{});
        if (found.isNull() || guard++ >= kMaxMatches) {
            break;
        }
        if (found.selectionEnd() <= found.selectionStart()) {
            break; // zero-width guard
        }
        matches_.append(found);
        from.setPosition(found.selectionEnd());
    }
    if (matches_.isEmpty()) {
        view_->setExtraSelections({});
        count_->setText(tr("no matches"));
        return;
    }
    current_ = 0;
}

void FindBar::showCurrent()
{
    if (view_ == nullptr || current_ < 0 || current_ >= matches_.size()) {
        return;
    }
    QList<QTextEdit::ExtraSelection> sels;
    sels.reserve(matches_.size());
    for (int i = 0; i < matches_.size(); ++i) {
        QTextEdit::ExtraSelection sel;
        sel.cursor = matches_.at(i);
        sel.format.setBackground(i == current_ ? kCurrentBack : kMatchBack);
        sel.format.setForeground(kMatchFront);
        sels.append(sel);
    }
    view_->setExtraSelections(sels);
    view_->setTextCursor(matches_.at(current_));
    view_->ensureCursorVisible();
    count_->setText(tr("%1/%2").arg(current_ + 1).arg(matches_.size()));
}

void FindBar::clearAll()
{
    matches_.clear();
    current_ = -1;
    if (view_ != nullptr) {
        view_->setExtraSelections({});
    }
    count_->clear();
}

} // namespace YuzNote
