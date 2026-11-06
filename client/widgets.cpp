#include "widgets.h"
#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QFontDatabase>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextTable>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <cmath>

namespace client {
QIcon makeIcon(const QString &kind, const QColor &color, int size) {
    QPixmap pixmap(size * 2, size * 2);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(size / 24.0 * 2, size / 24.0 * 2);
    p.setPen(QPen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    if (kind == "menu") {
        p.drawLine(5, 7, 19, 7);
        p.drawLine(5, 12, 19, 12);
        p.drawLine(5, 17, 19, 17);
    } else if (kind == "plus") {
        p.drawLine(12, 5, 12, 19);
        p.drawLine(5, 12, 19, 12);
    } else if (kind == "refresh") {
        p.drawArc(QRectF(5, 5, 14, 14), 45 * 16, 285 * 16);
        p.drawLine(19, 4, 19, 10);
        p.drawLine(19, 10, 13, 10);
    } else if (kind == "copy") {
        p.drawRoundedRect(QRectF(8, 8, 12, 12), 2, 2);
        p.drawLine(5, 15, 4, 15);
        p.drawLine(4, 15, 4, 4);
        p.drawLine(4, 4, 15, 4);
        p.drawLine(15, 4, 15, 5);
    } else if (kind == "send") {
        p.drawLine(12, 19, 12, 5);
        p.drawLine(6, 11, 12, 5);
        p.drawLine(12, 5, 18, 11);
    } else if (kind == "stop") {
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(QRectF(7, 7, 10, 10), 2, 2);
    } else if (kind == "export") {
        p.drawLine(12, 3, 12, 15);
        p.drawLine(7, 10, 12, 15);
        p.drawLine(12, 15, 17, 10);
        p.drawLine(5, 16, 5, 21);
        p.drawLine(5, 21, 19, 21);
        p.drawLine(19, 21, 19, 16);
    } else if (kind == "settings") {
        for (int i = 0; i < 3; ++i) {
            const int y = 6 + i * 6;
            p.drawLine(4, y, 20, y);
            p.setBrush(QColor("#f0ede5"));
            p.drawEllipse(QPointF(i == 1 ? 15 : 9, y), 2.5, 2.5);
        }
    } else if (kind == "search") {
        p.drawEllipse(QRectF(4, 4, 11, 11));
        p.drawLine(14, 14, 20, 20);
    } else if (kind == "trash") {
        p.drawLine(4, 6, 20, 6);
        p.drawLine(9, 3, 15, 3);
        p.drawLine(6, 6, 7, 21);
        p.drawLine(7, 21, 17, 21);
        p.drawLine(17, 21, 18, 6);
        p.drawLine(10, 10, 10, 17);
        p.drawLine(14, 10, 14, 17);
    } else if (kind == "close") {
        p.drawLine(6, 6, 18, 18);
        p.drawLine(18, 6, 6, 18);
    } else {
        p.drawLine(5, 19, 12, 5);
        p.drawLine(12, 5, 19, 19);
        p.drawLine(8, 14, 16, 14);
    }
    p.end();
    pixmap.setDevicePixelRatio(2);
    return QIcon(pixmap);
}
QIcon appIcon() {
    QIcon icon;
    for (int size : {32, 48, 64, 128, 256}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.scale(size / 64.0, size / 64.0);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#426650"));
        p.drawRoundedRect(QRectF(1, 1, 62, 62), 17, 17);
        p.setPen(QPen(QColor("#f9f5eb"), 3.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawLine(18, 45, 31, 17);
        p.drawLine(31, 17, 44, 45);
        p.drawLine(23, 35, 39, 35);
        p.setBrush(QColor("#d2dfb8"));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(49, 17), 4, 4);
        p.end();
        icon.addPixmap(pixmap);
    }
    return icon;
}
void applyTheme() {
    QApplication::setStyle("Fusion");
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#f5f2eb"));
    palette.setColor(QPalette::WindowText, QColor("#2e3b32"));
    palette.setColor(QPalette::Base, QColor("#fdfbf6"));
    palette.setColor(QPalette::Text, QColor("#2e3b32"));
    palette.setColor(QPalette::Button, QColor("#ecebe1"));
    palette.setColor(QPalette::ButtonText, QColor("#334539"));
    palette.setColor(QPalette::Highlight, QColor("#d6e3d2"));
    palette.setColor(QPalette::HighlightedText, QColor("#263a2c"));
    palette.setColor(QPalette::PlaceholderText, QColor("#8d9387"));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#a0a69a"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#9da697"));
    qApp->setPalette(palette);
    QString family = QApplication::font().family();
    for (const auto &candidate : {QString("Microsoft YaHei UI"), QString("Microsoft YaHei"),
                                  QString("Noto Sans CJK SC"), QString("Segoe UI")}) {
        if (QFontDatabase::hasFamily(candidate)) {
            family = candidate;
            break;
        }
    }
    QFont font(family, 10);
    font.setStyleStrategy(QFont::PreferAntialias);
    qApp->setFont(font);
    qApp->setStyleSheet(QStringLiteral(R"(
        QMainWindow, QWidget#MainArea, QWidget#ChatContent {background:#f5f2eb;}
        QWidget {color:#2e3b32;}
        QFrame#Sidebar {background:#edeae2;border-right:1px solid #deded3;}
        QLabel#Brand {font-size:17px;font-weight:600;color:#304938;}
        QLabel#SectionLabel {color:#8c9385;font-size:11px;}
        QLabel#Muted, QLabel#Footer {color:#858d80;font-size:11px;}
        QLabel#WelcomeTitle {font-size:29px;font-weight:600;color:#344a39;}
        QLabel#WelcomeText {color:#7e8879;font-size:14px;}
        QFrame#Topbar {background:#f5f2eb;border-bottom:1px solid #e3e1d7;}
        QFrame#Composer {background:#fdfbf6;border:1px solid #d8dccd;border-radius:18px;}
        QFrame#UserBubble {background:#e2eadd;border:1px solid #d6e0cf;border-radius:17px;}
        QFrame#AssistantCard {background:#fdfbf6;border:1px solid #e3e3d7;border-radius:17px;}
        QFrame#ErrorCard {background:#fbf0e8;border:1px solid #e9d6c5;border-radius:15px;}
        QFrame#Diagnostics {background:#f1f1e9;border:1px solid #e0e2d5;border-radius:10px;}
        QLabel#ModelBadge {background:#edf1e7;color:#597052;border-radius:5px;padding:4px 8px;font-size:11px;}
        QLabel#MockBadge {background:#f8eacb;color:#8c692c;border-radius:5px;padding:4px 8px;font-size:11px;}
        QLabel#FallbackNote {background:#f7efdb;color:#8b743c;border-radius:8px;padding:10px;font-size:12px;}
        QLabel#Notice {background:#f0e7d6;color:#7a653e;border-radius:9px;padding:10px;font-size:12px;}
        QLabel#Connection {font-size:11px;color:#688064;}
        QLabel#RoleTitle {font-weight:600;font-size:12px;color:#39503e;}
        QLabel#Avatar {background:#dce8d4;color:#426746;border-radius:10px;font-weight:600;font-size:12px;}
        QToolTip {background:#fdfbf6;color:#3a493d;border:1px solid #d8dccf;padding:6px;}
        QPushButton {background:#f4f3eb;border:1px solid #dce0d1;border-radius:9px;padding:8px 14px;}
        QPushButton:hover {background:#e8eddf;border-color:#c2d0b5;}
        QPushButton:pressed {background:#dce7d4;}
        QPushButton#NewChat {background:#dce7d3;border:1px solid #c7d7ba;text-align:left;padding:12px 14px;font-weight:600;}
        QPushButton#SendButton {background:#48694f;color:#f8f7ef;border:none;border-radius:11px;padding:10px 14px;font-weight:600;}
        QPushButton#SendButton:hover {background:#36593f;}
        QPushButton#SendButton:disabled {background:#dfe4d6;color:#96a08f;}
        QPushButton#Suggestion {background:#f9f7f0;border:1px solid #e1e2d6;border-radius:13px;text-align:left;padding:17px 18px;}
        QPushButton#Suggestion:hover {background:#eaf0e2;border-color:#ccd9c0;}
        QToolButton {background:transparent;border:1px solid transparent;border-radius:7px;padding:6px;}
        QToolButton:hover {background:#e6ebdd;border-color:#d8dfcc;}
        QToolButton#TextAction {font-size:11px;color:#7c8974;padding:4px 6px;}
        QToolButton#TextAction:checked {background:#e6ebdd;}
        QLineEdit {background:#f9f7ef;border:1px solid #dedfd1;border-radius:8px;padding:9px 10px;selection-background-color:#d2e1c7;}
        QLineEdit:focus {border-color:#94ae83;}
        QPlainTextEdit {background:transparent;border:none;padding:3px;selection-background-color:#d3e2c9;font-size:14px;}
        QComboBox {background:#faf8f1;border:1px solid #dcded0;border-radius:9px;padding:9px 12px;min-width:200px;font-size:12px;}
        QComboBox::drop-down {border:0;width:24px;}
        QComboBox QAbstractItemView {background:#fdfbf6;selection-background-color:#e1ead8;padding:5px;border:1px solid #d5ddc8;}
        QListWidget#History {background:transparent;border:none;outline:0;}
        QScrollArea {background:transparent;border:none;}
        QScrollArea>QWidget>QWidget {background:transparent;}
        QTextBrowser {background:transparent;border:none;selection-background-color:#d4e2c9;}
        QScrollBar:vertical {background:transparent;width:7px;margin:2px;}
        QScrollBar::handle:vertical {background:#cdd3c3;min-height:30px;border-radius:3px;}
        QScrollBar::handle:vertical:hover {background:#b8c3ab;}
        QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical {height:0;}
        QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical {background:none;}
        QScrollBar:horizontal {background:#f0f1e8;height:8px;margin:0;}
        QScrollBar::handle:horizontal {background:#c8d1be;min-width:30px;border-radius:3px;}
        QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal {width:0;}
        QProgressBar {border:0;background:#e9edde;border-radius:2px;max-height:3px;}
        QProgressBar::chunk {background:#a2b992;border-radius:2px;}
        QMenu {background:#fcfaf4;border:1px solid #d8ddce;padding:5px;}
        QMenu::item {padding:8px 22px;border-radius:4px;}
        QMenu::item:selected {background:#e2ead8;}
        QDialog {background:#f5f2eb;}
    )"));
}
InputEditor::InputEditor(QWidget *parent) : QPlainTextEdit(parent) {
    setObjectName("messageInput");
    setPlaceholderText("输入你的问题，让想法继续生长……");
    setTabChangesFocus(true);
    setFixedHeight(68);
    connect(document(), &QTextDocument::contentsChanged, this, [this] {
        const int height =
            qBound(68, int(document()->size().height() * fontMetrics().lineSpacing()) + 14, 160);
        if (height != this->height())
            setFixedHeight(height);
    });
}
void InputEditor::keyPressEvent(QKeyEvent *event) {
    if (!composing_ && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
        !(event->modifiers() & Qt::ShiftModifier)) {
        event->accept();
        emit submitRequested();
        return;
    }
    QPlainTextEdit::keyPressEvent(event);
}
void InputEditor::inputMethodEvent(QInputMethodEvent *event) {
    composing_ = !event->preeditString().isEmpty();
    QPlainTextEdit::inputMethodEvent(event);
}
MarkdownView::MarkdownView(QWidget *parent) : QTextBrowser(parent) {
    setFrameShape(QFrame::NoFrame);
    setOpenLinks(false);
    setOpenExternalLinks(false);
    setUndoRedoEnabled(false);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFont(QFont(qApp->font().family(), 11));
    document()->setDocumentMargin(0);
    document()->setDefaultFont(font());
    document()->setDefaultStyleSheet(
        "p {margin-top:5px;margin-bottom:12px;line-height:155%;} h1 {font-size:21px;} h2 {font-size:18px;} "
        "h3 {font-size:16px;} pre {background-color:#eff1e8;white-space:pre-wrap;} code "
        "{font-family:Consolas;color:#426249;} blockquote {color:#7b8574;} table {border-collapse:collapse;} "
        "td,th {padding:7px;border:1px solid #d7dfcd;} a {color:#496b4a;text-decoration:underline;}");
    connect(document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged, this, [this] {
        fitDocument();
    });
    connect(this, &QTextBrowser::anchorClicked, this, [](const QUrl &url) {
        if ((url.scheme() == "https" || url.scheme() == "http") && !url.host().isEmpty() &&
            url.userInfo().isEmpty())
            QDesktopServices::openUrl(url);
    });
}
QVariant MarkdownView::loadResource(int type, const QUrl &) {
    if (type == QTextDocument::ImageResource) {
        QImage image(1, 1, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        return image;
    }
    return {};
}
void MarkdownView::setAnswer(const QString &markdown) {
    auto features = QTextDocument::MarkdownFeatures(QTextDocument::MarkdownDialectGitHub);
    features |= QTextDocument::MarkdownNoHTML;
    document()->setMarkdown(markdown.left(180000), features);
    QVector<QPair<int, int>> codeRanges;
    int codeStart = -1, codeEnd = -1;
    for (auto block = document()->begin(); block.isValid(); block = block.next()) {
        QTextCursor cursor(block);
        auto format = block.blockFormat();
        if (format.hasProperty(QTextFormat::BlockCodeFence) ||
            format.hasProperty(QTextFormat::BlockCodeLanguage) || format.nonBreakableLines()) {
            if (codeStart < 0)
                codeStart = block.position();
            codeEnd = block.position() + block.length() - 1;
            format.clearBackground();
            format.setLeftMargin(0);
            format.setRightMargin(0);
            format.setTopMargin(0);
            format.setBottomMargin(0);
            format.setLineHeight(135, QTextBlockFormat::ProportionalHeight);
            cursor.setBlockFormat(format);
            cursor.select(QTextCursor::BlockUnderCursor);
            QTextCharFormat code;
            code.setFontFamilies({"Consolas", "Cascadia Mono"});
            code.setFontPointSize(10);
            code.setForeground(QColor("#3c5740"));
            cursor.mergeCharFormat(code);
        } else {
            if (codeStart >= 0) {
                codeRanges.append({codeStart, codeEnd});
                codeStart = -1;
            }
            format.setLineHeight(155, QTextBlockFormat::ProportionalHeight);
            cursor.setBlockFormat(format);
        }
    }
    if (codeStart >= 0)
        codeRanges.append({codeStart, codeEnd});
    // A frame fills the entire code area, including leading between text lines.
    // Coloring individual paragraphs produces the striping seen in the old client.
    for (auto it = codeRanges.crbegin(); it != codeRanges.crend(); ++it) {
        QTextCursor cursor(document());
        cursor.setPosition(it->first);
        cursor.setPosition(it->second, QTextCursor::KeepAnchor);
        QTextFrameFormat frame;
        frame.setBackground(QColor("#eff1e8"));
        frame.setPadding(12);
        frame.setMargin(8);
        frame.setBorder(1);
        frame.setBorderBrush(QColor("#dde3d2"));
        frame.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);
        cursor.insertFrame(frame);
    }
    fitDocument();
}
void MarkdownView::fitDocument() {
    if (adjusting_ || width() < 20)
        return;
    adjusting_ = true;
    document()->setTextWidth(qMax(80, viewport()->width()));
    const bool horizontal = document()->size().width() > viewport()->width() + 1;
    const int desired =
        qBound(24, int(std::ceil(document()->size().height())) + 8 + (horizontal ? 14 : 0), 24000);
    if (height() != desired)
        setFixedHeight(desired);
    adjusting_ = false;
}
void MarkdownView::resizeEvent(QResizeEvent *event) {
    QTextBrowser::resizeEvent(event);
    fitDocument();
}
void HistoryDelegate::paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    const auto rect = option.rect.adjusted(0, 2, -2, -2);
    if (option.state & QStyle::State_Selected) {
        p->setPen(Qt::NoPen);
        p->setBrush(QColor("#dfe8d5"));
        p->drawRoundedRect(rect, 10, 10);
    } else if (option.state & QStyle::State_MouseOver) {
        p->setPen(Qt::NoPen);
        p->setBrush(QColor("#e6e9dc"));
        p->drawRoundedRect(rect, 10, 10);
    }
    auto font = option.font;
    font.setPointSize(10);
    p->setFont(font);
    p->setPen(QColor("#42533f"));
    const int x = rect.left() + 12, w = rect.width() - 24;
    p->drawText(QRect(x, rect.top() + 9, w, 24), Qt::AlignLeft | Qt::AlignVCenter,
                QFontMetrics(font).elidedText(index.data().toString(), Qt::ElideRight, w));
    font.setPointSize(8);
    p->setFont(font);
    p->setPen(QColor("#919784"));
    p->drawText(QRect(x, rect.top() + 34, w, 18), Qt::AlignLeft | Qt::AlignVCenter,
                QFontMetrics(font).elidedText(index.data(Qt::UserRole + 1).toString(), Qt::ElideRight, w));
    p->restore();
}
namespace {
QLabel *plain(const QString &text, QWidget *parent = nullptr) {
    auto *label = new QLabel(text, parent);
    label->setTextFormat(Qt::PlainText);
    return label;
}
QStringList fencedCode(const QString &text) {
    QStringList blocks;
    QString code, fence;
    bool inside = false;
    for (const auto &line : text.split('\n')) {
        const auto trimmed = line.trimmed();
        if (!inside && (trimmed.startsWith("```") || trimmed.startsWith("~~~"))) {
            fence = trimmed.left(3);
            inside = true;
            code.clear();
        } else if (inside && trimmed.startsWith(fence)) {
            blocks.append(code);
            inside = false;
        } else if (inside)
            code += line + '\n';
    }
    if (inside && !code.isEmpty())
        blocks.append(code);
    return blocks;
}
} // namespace
MessageWidget::MessageWidget(const Message &m, QWidget *parent) : QWidget(parent) {
    setObjectName("message_" + m.id);
    setProperty("messageRole", m.role);
    setProperty("messageStatus", m.status);
    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(12);
    if (m.role == "user") {
        row->addStretch(1);
        auto *bubble = new QFrame;
        userBubble_ = bubble;
        bubble->setObjectName("UserBubble");
        bubble->setMaximumWidth(680);
        auto *layout = new QVBoxLayout(bubble);
        layout->setContentsMargins(19, 14, 19, 15);
        layout->setSpacing(5);
        auto *role = plain("你");
        role->setObjectName("Muted");
        layout->addWidget(role);
        auto *text = plain(m.content);
        text->setWordWrap(true);
        text->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        text->setFont(QFont(qApp->font().family(), 11));
        int lineWidth = 0;
        for (const auto &line : m.content.split('\n'))
            lineWidth = qMax(lineWidth, text->fontMetrics().horizontalAdvance(line));
        userPreferredWidth_ = qBound(150, lineWidth + 48, 680);
        layout->addWidget(text);
        row->addWidget(bubble, 0);
        return;
    }
    auto *avatar = plain("AI");
    avatar->setObjectName("Avatar");
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setFixedSize(32, 32);
    row->addWidget(avatar, 0, Qt::AlignTop);
    auto *card = new QFrame;
    card->setObjectName(m.status == "error" ? "ErrorCard" : "AssistantCard");
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(20, 16, 20, 17);
    layout->setSpacing(12);
    auto *header = new QHBoxLayout;
    auto *role = plain("AI Gateway");
    role->setObjectName("RoleTitle");
    header->addWidget(role);
    if (!m.modelName.isEmpty() && m.status == "complete") {
        auto *name = plain(m.modelName);
        name->setObjectName("ModelBadge");
        header->addWidget(name);
    }
    if (m.isMock && m.status == "complete") {
        auto *tag = plain("模拟演示");
        tag->setObjectName("MockBadge");
        header->addWidget(tag);
    }
    header->addStretch();
    layout->addLayout(header);
    if (m.status == "pending") {
        auto *waiting = plain("正在等待回答……");
        waiting->setObjectName("Muted");
        layout->addWidget(waiting);
        auto *progress = new QProgressBar;
        progress->setRange(0, 0);
        progress->setTextVisible(false);
        progress->setFixedHeight(3);
        layout->addWidget(progress);
        auto *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [waiting, created = m.created] {
            waiting->setText(
                "正在等待回答 · " +
                QString::number(qMax<qint64>(0, created.secsTo(QDateTime::currentDateTimeUtc()))) + " 秒");
        });
        timer->start(1000);
        row->addWidget(card, 1);
        return;
    }
    if (m.fallbackUsed) {
        auto *note = plain("原模型暂时不可用，已改由 " + m.modelName + " 回答。");
        note->setObjectName("FallbackNote");
        note->setWordWrap(true);
        layout->addWidget(note);
    }
    if (m.status == "complete") {
        auto *body = new MarkdownView;
        body->setObjectName("answerBody");
        body->setAnswer(m.content);
        layout->addWidget(body);
        if (m.content.size() > 180000) {
            auto *note = plain("回答较长，当前展示前一部分；复制回答或导出对话可获得完整内容。");
            note->setWordWrap(true);
            note->setObjectName("Muted");
            layout->addWidget(note);
        }
    } else {
        auto *body = plain(m.content);
        body->setWordWrap(true);
        body->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(body);
    }
    auto *actions = new QHBoxLayout;
    actions->setSpacing(12);
    auto *copy = new QToolButton;
    copy->setObjectName("TextAction");
    copy->setIcon(makeIcon("copy"));
    copy->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    copy->setText("复制回答");
    connect(copy, &QToolButton::clicked, this, [copy, content = m.content] {
        QApplication::clipboard()->setText(content);
        copy->setText("已复制");
        QTimer::singleShot(1800, copy, [copy] {
            copy->setText("复制回答");
        });
    });
    actions->addWidget(copy);
    const auto codeBlocks = fencedCode(m.content);
    if (!codeBlocks.isEmpty() && m.status == "complete") {
        auto *code = new QToolButton;
        code->setObjectName("TextAction");
        code->setText(codeBlocks.size() == 1 ? "复制代码" : "复制代码 ▾");
        if (codeBlocks.size() == 1)
            connect(code, &QToolButton::clicked, this, [code, codeBlocks] {
                QApplication::clipboard()->setText(codeBlocks.first());
                code->setText("已复制代码");
            });
        else {
            auto *menu = new QMenu(code);
            for (qsizetype i = 0; i < codeBlocks.size(); ++i) {
                auto *action = menu->addAction("代码 " + QString::number(i + 1));
                connect(action, &QAction::triggered, this, [text = codeBlocks[i]] {
                    QApplication::clipboard()->setText(text);
                });
            }
            code->setMenu(menu);
            code->setPopupMode(QToolButton::InstantPopup);
        }
        actions->addWidget(code);
    }
    if (!m.replyTo.isEmpty()) {
        auto *retry = new QToolButton;
        retry->setObjectName("TextAction");
        retry->setText("重新发送");
        connect(retry, &QToolButton::clicked, this, [this, id = m.replyTo] {
            emit retryRequested(id);
        });
        actions->addWidget(retry);
    }
    actions->addStretch();
    if (m.elapsedMs > 0) {
        auto *elapsed = plain(QString::number(m.elapsedMs / 1000.0, 'f', 1) + " 秒");
        elapsed->setObjectName("Muted");
        actions->addWidget(elapsed);
    }
    auto *details = new QToolButton;
    details->setObjectName("TextAction");
    details->setText("详情");
    details->setCheckable(true);
    actions->addWidget(details);
    layout->addLayout(actions);
    if (m.mode == "auto" && m.status == "complete") {
        auto *why = new QToolButton;
        why->setObjectName("TextAction");
        why->setText("▸ 为什么使用这个模型");
        why->setCheckable(true);
        layout->addWidget(why, 0, Qt::AlignLeft);
        QString summary = m.uncertain ? "问题包含多种线索，按通用问答配置选择了当前模型。"
                                      : "识别为" + taskName(m.task) + "问题，按当前配置优先选择此模型。";
        if (m.fallbackUsed)
            summary += "首选服务暂时不可用，本次使用备用模型完成回答。";
        auto *explanation = plain(summary);
        explanation->setObjectName("Muted");
        explanation->setWordWrap(true);
        explanation->hide();
        layout->addWidget(explanation);
        connect(why, &QToolButton::toggled, this, [why, explanation](bool expanded) {
            explanation->setVisible(expanded);
            why->setText(expanded ? "▾ 为什么使用这个模型" : "▸ 为什么使用这个模型");
        });
    }
    auto *diagnostic = new QFrame;
    diagnostic->setObjectName("Diagnostics");
    auto *form = new QFormLayout(diagnostic);
    form->setContentsMargins(12, 10, 12, 10);
    const auto add = [form](const QString &key, const QString &value) {
        if (value.isEmpty())
            return;
        auto *label = plain(value);
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        label->setFont(QFont(qApp->font().family(), 9));
        form->addRow(key, label);
    };
    add("问题类型", taskName(m.task) + (m.uncertain ? "（不确定）" : ""));
    add("内部编号", m.modelId);
    add("请求编号", m.requestId);
    add("判断依据", m.reason);
    add("错误代码", m.errorCode);
    if (m.httpStatus)
        add("HTTP", QString::number(m.httpStatus));
    auto *copyDetails = new QPushButton("复制诊断信息");
    form->addRow(copyDetails);
    connect(copyDetails, &QPushButton::clicked, this, [m] {
        QApplication::clipboard()->setText(
            "request_id=" + m.requestId + "\nmodel=" + m.modelId + "\nerror=" + m.errorCode +
            "\nHTTP=" + QString::number(m.httpStatus) + "\nreason=" + m.reason);
    });
    diagnostic->hide();
    layout->addWidget(diagnostic);
    connect(details, &QToolButton::toggled, diagnostic, &QWidget::setVisible);
    row->addWidget(card, 1);
}
void MessageWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (userBubble_)
        userBubble_->setFixedWidth(qMax(100, qMin(userPreferredWidth_, int(width() * 0.84))));
}
} // namespace client
