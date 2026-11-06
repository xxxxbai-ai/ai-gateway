#include "mainwindow.h"
#include "gateway_client.h"
#include "widgets.h"
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QSignalBlocker>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

namespace client {
namespace {
QLabel *label(const QString &text, const QString &name = {}) {
    auto *item = new QLabel(text);
    item->setTextFormat(Qt::PlainText);
    if (!name.isEmpty())
        item->setObjectName(name);
    return item;
}
QToolButton *tool(const QString &icon, const QString &tooltip, const QString &name = {}) {
    auto *button = new QToolButton;
    button->setIcon(makeIcon(icon));
    button->setIconSize({20, 20});
    button->setToolTip(tooltip);
    button->setAccessibleName(tooltip);
    if (!name.isEmpty())
        button->setObjectName(name);
    return button;
}
QString shortDate(const QDateTime &date) {
    const auto local = date.toLocalTime();
    const auto today = QDate::currentDate();
    if (local.date() == today)
        return "今天 · " + local.toString("HH:mm");
    if (local.date() == today.addDays(-1))
        return "昨天 · " + local.toString("HH:mm");
    return local.toString("MM月dd日 · HH:mm");
}
QString selectedChoice(QComboBox *combo) {
    return combo->currentData().toString().isEmpty() ? QString("auto") : combo->currentData().toString();
}
} // namespace
MainWindow::MainWindow(QString directory, QUrl overrideUrl, QWidget *parent)
    : QMainWindow(parent), store_(std::move(directory)) {
    QDir().mkpath(store_.directory());
    settings_ = std::make_unique<QSettings>(QDir(store_.directory()).filePath("preferences.ini"),
                                            QSettings::IniFormat);
    QString warning;
    store_.load(&warning);
    auto base =
        overrideUrl.isEmpty()
            ? GatewayClient::normalizeUrl(settings_->value("gateway", "http://127.0.0.1:8080").toString())
            : overrideUrl;
    if (base.isEmpty())
        base = QUrl("http://127.0.0.1:8080/");
    gateway_ = new GatewayClient(base, this);
    setWindowTitle("AI Gateway");
    setWindowIcon(appIcon());
    resize(1320, 920);
    setMinimumSize(980, 700);
    setupUi();
    connect(gateway_, &GatewayClient::modelsReady, this, [this](const QJsonArray &entries) {
        const auto old = selectedChoice(models_);
        const QSignalBlocker blocker(models_);
        modelNames_.clear();
        models_->clear();
        models_->addItem("自动选择模型", "auto");
        for (const auto &value : entries) {
            const auto item = value.toObject();
            const auto id = item["id"].toString();
            const auto name = friendlyName(item["name"].toString());
            modelNames_.insert(id, name);
            models_->addItem(name + (item["is_mock"].toBool() ? " · 模拟" : ""), id);
        }
        auto *c = store_.find(currentId_);
        const auto choice = c ? c->selectedModel : old;
        const int index = models_->findData(choice);
        models_->setCurrentIndex(index < 0 ? 0 : index);
        if (index < 0 && choice != "auto") {
            if (c)
                c->selectedModel = "auto";
            showNotice("之前选择的模型当前不可用，已回到自动选择；下次发送前可重新选择。");
        } else
            notice_->hide();
        modelsReady_ = !entries.isEmpty();
        connection_->setText(entries.isEmpty() ? "暂无可用模型"
                                               : "● 已连接 · " + QString::number(entries.size()) + " 个模型");
        if (entries.isEmpty())
            showNotice("网关已连接，但没有可用模型。请在网关配置中启用模型后刷新。");
        refresh_->setEnabled(true);
        refresh_->setToolTip("刷新模型列表");
        updateSendingState();
        persist();
    });
    connect(gateway_, &GatewayClient::modelsFailed, this, [this](const QString &text) {
        modelsReady_ = false;
        connection_->setText("○ 未连接");
        refresh_->setEnabled(true);
        refresh_->setToolTip("重新连接网关");
        showNotice(text);
        updateSendingState();
    });
    connect(gateway_, &GatewayClient::chatSucceeded, this, &MainWindow::onSuccess);
    connect(gateway_, &GatewayClient::chatFailed, this, &MainWindow::onFailure);
    currentId_ = settings_->value("lastConversation").toString();
    if (!store_.find(currentId_))
        currentId_ = store_.conversations.isEmpty() ? store_.create() : store_.conversations.first().id;
    rebuildHistory();
    renderConversation();
    sidebar_->setVisible(settings_->value("sidebarVisible", true).toBool());
    if (settings_->contains("geometry"))
        restoreGeometry(settings_->value("geometry").toByteArray());
    if (!warning.isEmpty())
        showNotice(warning);
    QTimer::singleShot(0, this, &MainWindow::refreshModels);
}
MainWindow::~MainWindow() = default;
void MainWindow::setupUi() {
    auto *root = new QWidget;
    auto *rootLayout = new QHBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    sidebar_ = new QFrame;
    sidebar_->setObjectName("Sidebar");
    sidebar_->setFixedWidth(244);
    auto *side = new QVBoxLayout(sidebar_);
    side->setContentsMargins(18, 26, 17, 18);
    side->setSpacing(12);
    auto *brand = new QHBoxLayout;
    auto *logo = label("");
    logo->setPixmap(appIcon().pixmap(34, 34));
    logo->setFixedSize(34, 34);
    brand->addWidget(logo);
    brand->addSpacing(5);
    brand->addWidget(label("AI Gateway", "Brand"));
    brand->addStretch();
    side->addLayout(brand);
    side->addSpacing(18);
    auto *create = new QPushButton("新建对话");
    create->setObjectName("NewChat");
    create->setIcon(makeIcon("plus"));
    create->setIconSize({18, 18});
    create->setMinimumHeight(46);
    create->setShortcut(QKeySequence("Ctrl+N"));
    side->addWidget(create);
    connect(create, &QPushButton::clicked, this, &MainWindow::newConversation);
    search_ = new QLineEdit;
    search_->setObjectName("historySearch");
    search_->setPlaceholderText("搜索对话");
    search_->setClearButtonEnabled(true);
    search_->addAction(makeIcon("search"), QLineEdit::LeadingPosition);
    side->addWidget(search_);
    side->addSpacing(7);
    side->addWidget(label("你的对话", "SectionLabel"));
    history_ = new QListWidget;
    history_->setObjectName("History");
    history_->setItemDelegate(new HistoryDelegate(history_));
    history_->setMouseTracking(true);
    history_->setUniformItemSizes(true);
    history_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    history_->setContextMenuPolicy(Qt::CustomContextMenu);
    side->addWidget(history_, 1);
    connect(search_, &QLineEdit::textChanged, this, [this] {
        rebuildHistory();
    });
    connect(history_, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem *item, QListWidgetItem *) {
                if (item)
                    switchConversation(item->data(Qt::UserRole).toString());
            });
    connect(history_, &QListWidget::customContextMenuRequested, this, &MainWindow::showHistoryMenu);
    auto *sideBottom = new QHBoxLayout;
    auto *version = label("桌面版 1.0", "Muted");
    sideBottom->addWidget(version);
    sideBottom->addStretch();
    auto *settings = tool("settings", "连接与设置", "settingsButton");
    sideBottom->addWidget(settings);
    connect(settings, &QToolButton::clicked, this, &MainWindow::showSettings);
    side->addLayout(sideBottom);
    rootLayout->addWidget(sidebar_);
    auto *main = new QWidget;
    main->setObjectName("MainArea");
    auto *mainLayout = new QVBoxLayout(main);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    auto *top = new QFrame;
    top->setObjectName("Topbar");
    top->setFixedHeight(78);
    auto *topLayout = new QHBoxLayout(top);
    topLayout->setContentsMargins(27, 12, 27, 12);
    topLayout->setSpacing(12);
    auto *toggle = tool("menu", "显示或收起侧栏", "sidebarToggle");
    connect(toggle, &QToolButton::clicked, this, [this] {
        sidebar_->setVisible(!sidebar_->isVisible());
        settings_->setValue("sidebarVisible", sidebar_->isVisible());
    });
    topLayout->addWidget(toggle);
    models_ = new QComboBox;
    models_->setObjectName("modelSelector");
    models_->setAccessibleName("选择模型");
    models_->setMinimumWidth(240);
    models_->setMaximumWidth(340);
    models_->addItem("自动选择模型", "auto");
    models_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    topLayout->addWidget(models_);
    connect(models_, &QComboBox::currentIndexChanged, this, [this] {
        if (auto *c = store_.find(currentId_)) {
            c->selectedModel = selectedChoice(models_);
            persist();
        }
    });
    refresh_ = tool("refresh", "刷新模型列表", "refreshModels");
    connect(refresh_, &QToolButton::clicked, this, &MainWindow::refreshModels);
    topLayout->addWidget(refresh_);
    topLayout->addStretch();
    connection_ = label("正在连接……", "Connection");
    topLayout->addWidget(connection_);
    topLayout->addSpacing(10);
    auto *exportButton = tool("export", "导出当前对话", "exportButton");
    connect(exportButton, &QToolButton::clicked, this, &MainWindow::exportCurrent);
    topLayout->addWidget(exportButton);
    mainLayout->addWidget(top);
    auto *noticeRow = new QWidget;
    auto *noticeLayout = new QHBoxLayout(noticeRow);
    noticeLayout->setContentsMargins(28, 0, 28, 0);
    notice_ = label("", "Notice");
    notice_->setWordWrap(true);
    notice_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    notice_->hide();
    noticeLayout->addWidget(notice_);
    mainLayout->addWidget(noticeRow);
    scroll_ = new QScrollArea;
    scroll_->setObjectName("chatScroll");
    scroll_->setWidgetResizable(true);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget;
    content->setObjectName("ChatContent");
    auto *readingRow = new QHBoxLayout(content);
    readingRow->setContentsMargins(30, 25, 30, 20);
    readingRow->setSpacing(0);
    reader_ = new QWidget;
    reader_->setMinimumWidth(0);
    reader_->setMaximumWidth(880);
    reader_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    messages_ = new QVBoxLayout(reader_);
    messages_->setContentsMargins(0, 0, 0, 0);
    messages_->setSpacing(25);
    readingRow->addStretch();
    readingRow->addWidget(reader_, 1);
    readingRow->addStretch();
    scroll_->setWidget(content);
    mainLayout->addWidget(scroll_, 1);
    auto *composeArea = new QWidget;
    auto *composeRow = new QHBoxLayout(composeArea);
    composeRow->setContentsMargins(30, 8, 30, 16);
    composeRow->setSpacing(0);
    auto *composeColumn = new QWidget;
    composeColumn->setMaximumWidth(880);
    auto *composeLayout = new QVBoxLayout(composeColumn);
    composeLayout->setContentsMargins(0, 0, 0, 0);
    composeLayout->setSpacing(9);
    auto *composer = new QFrame;
    composer->setObjectName("Composer");
    auto *inputLayout = new QVBoxLayout(composer);
    inputLayout->setContentsMargins(17, 13, 13, 12);
    inputLayout->setSpacing(3);
    input_ = new InputEditor;
    inputLayout->addWidget(input_);
    connect(input_, &InputEditor::submitRequested, this, &MainWindow::sendCurrent);
    auto *inputRow = new QHBoxLayout;
    inputHint_ = label("Enter 发送 · Shift + Enter 换行", "Muted");
    inputRow->addWidget(inputHint_);
    inputRow->addStretch();
    send_ = new QPushButton("发送");
    send_->setObjectName("SendButton");
    send_->setIcon(makeIcon("send", QColor("#f8f7ef"), 18));
    send_->setIconSize({18, 18});
    send_->setMinimumWidth(96);
    send_->setMinimumHeight(39);
    send_->setEnabled(false);
    connect(send_, &QPushButton::clicked, this, [this] {
        if (pendingToken_)
            gateway_->cancelChat();
        else
            sendCurrent();
    });
    inputRow->addWidget(send_);
    inputLayout->addLayout(inputRow);
    composeLayout->addWidget(composer);
    auto *footer = label("每次问题独立回答 · 历史仅保存在本机 · 重要信息请核对", "Footer");
    footer->setAlignment(Qt::AlignCenter);
    composeLayout->addWidget(footer);
    composeRow->addStretch();
    composeRow->addWidget(composeColumn, 1);
    composeRow->addStretch();
    mainLayout->addWidget(composeArea);
    rootLayout->addWidget(main, 1);
    setCentralWidget(root);
    connect(input_, &QPlainTextEdit::textChanged, this, &MainWindow::updateSendingState);
    auto *searchShortcut = new QShortcut(QKeySequence("Ctrl+K"), this);
    connect(searchShortcut, &QShortcut::activated, this, [this] {
        sidebar_->show();
        search_->setFocus();
    });
    auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escape, &QShortcut::activated, this, [this] {
        if (pendingToken_)
            gateway_->cancelChat();
    });
}
void MainWindow::showNotice(const QString &text) {
    notice_->setProperty("conversation", QString());
    notice_->setText(text);
    notice_->setVisible(!text.isEmpty());
}
void MainWindow::persist() {
    QString error;
    if (!store_.save(&error))
        showNotice(error);
    settings_->setValue("lastConversation", currentId_);
}
void MainWindow::rebuildHistory() {
    const QSignalBlocker block(history_);
    history_->clear();
    QVector<const Conversation *> items;
    for (const auto &c : store_.conversations)
        items.append(&c);
    std::stable_sort(items.begin(), items.end(), [](const auto *a, const auto *b) {
        return a->updated > b->updated;
    });
    const auto query = search_->text().trimmed();
    for (const auto *c : items) {
        bool match = query.isEmpty() || c->title.contains(query, Qt::CaseInsensitive);
        if (!match)
            for (const auto &m : c->messages)
                if (m.content.contains(query, Qt::CaseInsensitive)) {
                    match = true;
                    break;
                }
        if (!match)
            continue;
        auto *item = new QListWidgetItem(c->title, history_);
        item->setData(Qt::UserRole, c->id);
        item->setData(Qt::UserRole + 1,
                      c->id == pendingConversation_ ? "正在等待回答……" : shortDate(c->updated));
        item->setToolTip(c->title + "\n右键可以重命名或删除");
        if (c->id == currentId_)
            history_->setCurrentItem(item);
    }
}
void MainWindow::renderConversation(bool toEnd) {
    const auto previous = scroll_->verticalScrollBar()->value();
    while (auto *item = messages_->takeAt(0)) {
        if (item->widget())
            delete item->widget();
        delete item;
    }
    const auto *c = store_.find(currentId_);
    if (!c)
        return;
    {
        const QSignalBlocker block(models_);
        const int index = models_->findData(c->selectedModel);
        models_->setCurrentIndex(index < 0 ? 0 : index);
    }
    if (c->messages.isEmpty()) {
        auto *welcome = new QWidget;
        auto *layout = new QVBoxLayout(welcome);
        layout->setContentsMargins(15, 55, 15, 25);
        layout->setSpacing(17);
        auto *mark = label("");
        mark->setPixmap(appIcon().pixmap(54, 54));
        layout->addWidget(mark);
        layout->addSpacing(8);
        layout->addWidget(label("让每个问题，都有好的开始。", "WelcomeTitle"));
        auto *sub = label("写代码、解题、翻译或写作，从一个想法开始。", "WelcomeText");
        sub->setWordWrap(true);
        layout->addWidget(sub);
        layout->addSpacing(22);
        auto *grid = new QGridLayout;
        grid->setSpacing(13);
        const QVector<QPair<QString, QString>> examples{
            {"写一段代码", "请用 C++ 写一个二分查找函数，并解释它的思路。"},
            {"把难题讲清楚", "请用简单例子解释一元二次方程的求根公式。"},
            {"找到准确的表达", "请把“很高兴收到你的来信”翻译成自然的英文。"},
            {"让文字更好一点", "请帮我写一封简洁、有礼貌的请假邮件。"}};
        for (int i = 0; i < examples.size(); ++i) {
            auto *button = new QPushButton(examples[i].first + "\n\n" +
                                           QStringList{"从思路走到可运行的程序", "一步一步理解数学问题",
                                                       "用另一种语言说得自然", "整理思路，让表达更清楚"}[i]);
            button->setObjectName("Suggestion");
            button->setMinimumHeight(104);
            grid->addWidget(button, i / 2, i % 2);
            connect(button, &QPushButton::clicked, this, [this, text = examples[i].second] {
                input_->setPlainText(text);
                input_->setFocus();
            });
        }
        layout->addLayout(grid);
        messages_->addWidget(welcome);
    } else {
        auto *date = label(c->created.toLocalTime().toString("yyyy年M月d日"), "Muted");
        date->setAlignment(Qt::AlignCenter);
        messages_->addWidget(date);
        for (const auto &m : c->messages) {
            auto *widget = new MessageWidget(m);
            connect(widget, &MessageWidget::retryRequested, this, &MainWindow::retryQuestion);
            messages_->addWidget(widget);
        }
    }
    messages_->addStretch();
    updateSendingState();
    QTimer::singleShot(50, this, [this, toEnd, previous] {
        scroll_->verticalScrollBar()->setValue(toEnd ? scroll_->verticalScrollBar()->maximum() : previous);
    });
}
void MainWindow::switchConversation(const QString &id) {
    if (id == currentId_ || !store_.find(id))
        return;
    drafts_[currentId_] = input_->toPlainText();
    currentId_ = id;
    if (notice_->property("conversation").toString() == id)
        notice_->hide();
    input_->setPlainText(drafts_.value(id));
    if (auto *c = store_.find(id);
        c && modelsReady_ && c->selectedModel != "auto" && !modelNames_.contains(c->selectedModel)) {
        c->selectedModel = "auto";
        persist();
        showNotice("此对话原来使用的模型当前不可用，已回到自动选择。");
    }
    renderConversation();
    settings_->setValue("lastConversation", id);
    input_->setFocus();
}
void MainWindow::newConversation() {
    if (auto *current = store_.find(currentId_); current && current->messages.isEmpty()) {
        input_->setFocus();
        return;
    }
    drafts_[currentId_] = input_->toPlainText();
    currentId_ = store_.create();
    input_->clear();
    search_->clear();
    rebuildHistory();
    renderConversation();
    persist();
    input_->setFocus();
}
void MainWindow::refreshModels() {
    modelsReady_ = false;
    refresh_->setEnabled(false);
    connection_->setText("正在连接……");
    updateSendingState();
    gateway_->fetchModels();
}
void MainWindow::updateSendingState() {
    const bool pending = pendingToken_ != 0;
    send_->setText(pending ? "停止等待" : "发送");
    send_->setIcon(makeIcon(pending ? "stop" : "send", QColor("#f8f7ef"), 18));
    send_->setEnabled(pending || (modelsReady_ && !input_->toPlainText().trimmed().isEmpty()));
    models_->setEnabled(modelsReady_);
    inputHint_->setText(pending ? (pendingConversation_ == currentId_ ? "正在等待回答 · 可停止等待"
                                                                      : "另一对话正在等待，可先写下新问题")
                                : "Enter 发送 · Shift + Enter 换行");
}
void MainWindow::sendCurrent() {
    if (pendingToken_) {
        showNotice("已有一个问题正在等待回答。可以继续编辑，或点击“停止等待”。");
        return;
    }
    sendQuestion(input_->toPlainText(), selectedChoice(models_));
}
void MainWindow::sendQuestion(const QString &question, const QString &choice) {
    if (pendingToken_)
        return;
    if (!modelsReady_) {
        showNotice("请先启动网关并刷新模型列表。");
        return;
    }
    const auto text = question.trimmed();
    if (text.isEmpty())
        return;
    if (text.toUtf8().size() > 512 * 1024) {
        showNotice("这次输入过长，请分成几个问题发送。");
        return;
    }
    if (choice != "auto" && !modelNames_.contains(choice)) {
        showNotice("这个模型当前不可用，请重新选择模型。");
        return;
    }
    auto *c = store_.find(currentId_);
    if (!c)
        return;
    Message user;
    user.id = newId();
    user.role = "user";
    user.content = text;
    user.mode = choice == "auto" ? "auto" : "manual";
    user.modelId = choice;
    Message assistant;
    assistant.id = newId();
    assistant.role = "assistant";
    assistant.status = "pending";
    assistant.replyTo = user.id;
    assistant.mode = user.mode;
    if (choice != "auto")
        assistant.modelId = choice;
    if (!c->customTitle && c->messages.isEmpty()) {
        auto title = text;
        title.replace(QRegularExpression("\\s+"), " ");
        c->title = title.left(22) + (title.size() > 22 ? "…" : "");
    }
    c->selectedModel = choice;
    c->messages.append(user);
    c->messages.append(assistant);
    c->updated = QDateTime::currentDateTimeUtc();
    pendingConversation_ = c->id;
    pendingMessage_ = assistant.id;
    pendingToken_ = ++nextToken_;
    pendingNames_ = modelNames_;
    input_->clear();
    drafts_.remove(currentId_);
    notice_->hide();
    rebuildHistory();
    renderConversation();
    persist();
    gateway_->sendChat(pendingToken_,
                       {{"mode", user.mode},
                        {"model", choice == "auto" ? QJsonValue(QJsonValue::Null) : QJsonValue(choice)},
                        {"message", text}});
}
void MainWindow::retryQuestion(const QString &messageId) {
    if (pendingToken_) {
        showNotice("请先结束当前等待，再重新发送。");
        return;
    }
    if (auto *message = store_.findMessage(currentId_, messageId))
        sendQuestion(message->content, message->modelId.isEmpty() ? QString("auto") : message->modelId);
}
void MainWindow::onSuccess(quint64 token, const QJsonObject &body, qint64 elapsed) {
    if (token != pendingToken_ || !pendingToken_)
        return;
    const auto conversation = pendingConversation_;
    auto *message = store_.findMessage(conversation, pendingMessage_);
    if (message) {
        message->status = "complete";
        message->content = body["answer"].toString();
        message->modelId = body["selected_model"].toString();
        message->modelName = pendingNames_.value(message->modelId, "网关模型");
        message->task = body["task"].toString();
        message->reason = body["reason"].toString();
        message->requestId = body["request_id"].toString();
        message->isMock = body["is_mock"].toBool();
        message->fallbackUsed = body["fallback_used"].toBool();
        message->uncertain = body["uncertain"].toBool();
        message->elapsedMs = elapsed;
        message->httpStatus = 200;
    }
    finishRequest(conversation);
}
void MainWindow::onFailure(quint64 token, const QString &text, const QJsonObject &details) {
    if (token != pendingToken_ || !pendingToken_)
        return;
    const auto conversation = pendingConversation_;
    auto *message = store_.findMessage(conversation, pendingMessage_);
    if (message) {
        message->status = details["error_code"] == "cancelled" ? "cancelled" : "error";
        message->content = text;
        message->errorCode = details["error_code"].toString();
        message->requestId = details["request_id"].toString();
        message->httpStatus = details["http_status"].toInt();
        message->elapsedMs = details["elapsed_ms"].toInteger();
    }
    finishRequest(conversation);
}
void MainWindow::finishRequest(const QString &conversation) {
    if (auto *c = store_.find(conversation))
        c->updated = QDateTime::currentDateTimeUtc();
    pendingToken_ = 0;
    pendingConversation_.clear();
    pendingMessage_.clear();
    pendingNames_.clear();
    persist();
    rebuildHistory();
    if (currentId_ == conversation)
        renderConversation();
    else if (const auto *c = store_.find(conversation)) {
        showNotice("「" + c->title + "」的等待已结束，可在左侧查看。");
        notice_->setProperty("conversation", conversation);
    }
    updateSendingState();
}
void MainWindow::showSettings() {
    QDialog dialog(this);
    dialog.setWindowTitle("连接与设置");
    dialog.setMinimumWidth(530);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(25, 22, 25, 22);
    layout->setSpacing(18);
    auto *title = label("连接你的网关");
    title->setFont(QFont(qApp->font().family(), 15, QFont::DemiBold));
    layout->addWidget(title);
    auto *form = new QFormLayout;
    auto *address = new QLineEdit(gateway_->baseUrl().toString());
    address->setObjectName("gatewayAddress");
    form->addRow("网关地址", address);
    layout->addLayout(form);
    auto *hint = label(pendingToken_ ? "当前正在等待回答，结束等待后可以更改连接地址。"
                                     : "客户端只连接网关。模型平台的密钥在网关中配置。",
                       "Muted");
    hint->setWordWrap(true);
    layout->addWidget(hint);
    address->setReadOnly(pendingToken_ != 0);
    auto *path = label("本机历史目录\n" + QDir::toNativeSeparators(store_.directory()), "Muted");
    path->setWordWrap(true);
    path->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(path);
    auto *privacy = label("历史以本机文件保存。当前网关按单条问题回答，不会读取这些历史。", "Muted");
    privacy->setWordWrap(true);
    layout->addWidget(privacy);
    auto *open = new QPushButton("打开历史文件夹");
    connect(open, &QPushButton::clicked, this, [this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(store_.directory()));
    });
    layout->addWidget(open, 0, Qt::AlignLeft);
    auto *error = label("");
    error->setWordWrap(true);
    error->setStyleSheet("color:#a96943;");
    layout->addWidget(error);
    auto *buttons = new QDialogButtonBox;
    auto *save = buttons->addButton("保存并连接", QDialogButtonBox::AcceptRole);
    save->setEnabled(!pendingToken_);
    buttons->addButton("关闭", QDialogButtonBox::RejectRole);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(save, &QPushButton::clicked, &dialog, [this, &dialog, address, error] {
        QString why;
        const auto url = GatewayClient::normalizeUrl(address->text(), &why);
        if (url.isEmpty()) {
            error->setText(why);
            return;
        }
        settings_->setValue("gateway", url.toString());
        gateway_->setBaseUrl(url);
        dialog.accept();
        refreshModels();
    });
    layout->addWidget(buttons);
    dialog.exec();
}
void MainWindow::exportCurrent() {
    const auto *conversation = store_.find(currentId_);
    if (!conversation || conversation->messages.isEmpty()) {
        showNotice("当前还没有可导出的消息。");
        return;
    }
    auto name = conversation->title;
    name.replace(QRegularExpression("[<>:\"/\\\\|?*]"), "_");
    const auto path = QFileDialog::getSaveFileName(this, "导出对话", name + ".md", "Markdown 文件 (*.md)");
    if (path.isEmpty())
        return;
    QSaveFile file(path);
    const auto bytes = exportMarkdown(*conversation).toUtf8();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        showNotice("导出失败，请检查保存位置。");
        return;
    }
    showNotice("对话已导出。");
}
void MainWindow::showHistoryMenu(const QPoint &point) {
    const auto *item = history_->itemAt(point);
    if (!item)
        return;
    const auto id = item->data(Qt::UserRole).toString();
    QMenu menu(this);
    auto *rename = menu.addAction("重命名");
    auto *remove = menu.addAction("删除对话");
    remove->setEnabled(id != pendingConversation_);
    const auto *chosen = menu.exec(history_->viewport()->mapToGlobal(point));
    if (!chosen)
        return;
    if (chosen == rename) {
        auto *c = store_.find(id);
        if (!c)
            return;
        QInputDialog renameDialog(this);
        renameDialog.setWindowTitle("重命名对话");
        renameDialog.setLabelText("对话名称");
        renameDialog.setTextValue(c->title);
        renameDialog.setOkButtonText("保存");
        renameDialog.setCancelButtonText("取消");
        const bool ok = renameDialog.exec() == QDialog::Accepted;
        const auto name = renameDialog.textValue().trimmed();
        if (ok && !name.isEmpty()) {
            c->title = name.left(80);
            c->customTitle = true;
            persist();
            rebuildHistory();
        }
    } else if (chosen == remove) {
        QMessageBox question(QMessageBox::Question, "删除对话", "删除这段本机对话记录？",
                             QMessageBox::NoButton, this);
        auto *yes = question.addButton("删除", QMessageBox::DestructiveRole);
        question.addButton("取消", QMessageBox::RejectRole);
        question.exec();
        if (question.clickedButton() != yes)
            return;
        store_.remove(id);
        drafts_.remove(id);
        if (currentId_ == id) {
            currentId_ = store_.conversations.isEmpty() ? store_.create() : store_.conversations.first().id;
            input_->setPlainText(drafts_.value(currentId_));
            renderConversation();
        }
        persist();
        rebuildHistory();
    }
}
void MainWindow::closeEvent(QCloseEvent *event) {
    if (pendingToken_)
        gateway_->cancelChat();
    persist();
    settings_->setValue("geometry", saveGeometry());
    settings_->setValue("sidebarVisible", sidebar_->isVisible());
    settings_->sync();
    QMainWindow::closeEvent(event);
}
} // namespace client
