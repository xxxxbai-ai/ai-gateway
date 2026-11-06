#pragma once
#include "conversation.h"
#include <QIcon>
#include <QPlainTextEdit>
#include <QStyledItemDelegate>
#include <QTextBrowser>
#include <QWidget>

namespace client {
QIcon makeIcon(const QString &kind, const QColor &color = QColor("#48604e"), int size = 20);
QIcon appIcon();
void applyTheme();
class InputEditor : public QPlainTextEdit {
    Q_OBJECT
  public:
    explicit InputEditor(QWidget *parent = nullptr);
  signals:
    void submitRequested();

  protected:
    void keyPressEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;

  private:
    bool composing_ = false;
};
class MarkdownView : public QTextBrowser {
    Q_OBJECT
  public:
    explicit MarkdownView(QWidget *parent = nullptr);
    void setAnswer(const QString &markdown);
    QVariant loadResource(int type, const QUrl &name) override;

  protected:
    void resizeEvent(QResizeEvent *event) override;

  private:
    bool adjusting_ = false;
    void fitDocument();
};
class HistoryDelegate : public QStyledItemDelegate {
  public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override {
        return {220, 66};
    }
};
class MessageWidget : public QWidget {
    Q_OBJECT
  public:
    explicit MessageWidget(const Message &message, QWidget *parent = nullptr);
  signals:
    void retryRequested(const QString &userMessageId);

  protected:
    void resizeEvent(QResizeEvent *event) override;

  private:
    QWidget *userBubble_ = nullptr;
    int userPreferredWidth_ = 200;
};
} // namespace client
