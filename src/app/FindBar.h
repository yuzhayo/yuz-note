#pragma once

#include <QList>
#include <QTextCursor>
#include <QWidget>

class QLineEdit;
class QLabel;
class QTextBrowser;

namespace YuzNote {

// Slim find bar bound to any QTextBrowser (PLAN P4). Case-insensitive,
// highlight-all with current-match emphasis; Enter/Shift+Enter cycle,
// Esc closes.
class FindBar : public QWidget {
    Q_OBJECT
public:
    explicit FindBar(QTextBrowser *view, QWidget *parent = nullptr);

    void activate(); // show + focus + select all

signals:
    void closed();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onTextChanged(const QString &text);
    void findNext();
    void findPrevious();

private:
    void rebuild();
    void showCurrent();
    void clearAll();

    QTextBrowser *view_ = nullptr;
    QLineEdit *edit_ = nullptr;
    QLabel *count_ = nullptr;
    QList<QTextCursor> matches_;
    int current_ = -1;
};

} // namespace YuzNote
