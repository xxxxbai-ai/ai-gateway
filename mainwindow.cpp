#include "mainwindow.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkRequest>
#include <QUrl>
#include <QListWidget>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), isSidebarVisible(true)
{
    setWindowTitle("AIGateway 客户端 v0.3 [本地模拟版]");
    resize(950, 650);

    networkManager = new QNetworkAccessManager(this);
    connect(networkManager, &QNetworkAccessManager::finished, this, &MainWindow::onNetworkReply);

    modelManager = new QNetworkAccessManager(this);
    connect(modelManager, &QNetworkAccessManager::finished, this, &MainWindow::onModelsReply);

    timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);
    connect(timeoutTimer, &QTimer::timeout, this, &MainWindow::onRequestTimeout);

    setupUI();
    applyModernStyle();
    fetchModelList();
}

MainWindow::~MainWindow() {}

void MainWindow::setupUI()
{
    centralWidget = new QWidget(this);
    QHBoxLayout *rootLayout = new QHBoxLayout(centralWidget);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    sidebarWidget = new QWidget(this);
    sidebarWidget->setObjectName("SidebarWidget");
    QVBoxLayout *sidebarLayout = new QVBoxLayout(sidebarWidget);
    sidebarLayout->setContentsMargins(12, 16, 12, 16);
    sidebarLayout->setSpacing(12);

    newChatButton = new QPushButton("+ 新建对话", this);
    newChatButton->setObjectName("NewChatButton");
    sidebarLayout->addWidget(newChatButton);

    QLabel *historyLabel = new QLabel("历史会话", this);
    historyLabel->setObjectName("SidebarTitleLabel");
    sidebarLayout->addWidget(historyLabel);

    historyListWidget = new QListWidget(this);
    historyListWidget->setObjectName("HistoryListWidget");
    historyListWidget->addItem("默认 AI 对话会话 (模拟)");
    sidebarLayout->addWidget(historyListWidget);

    rootLayout->addWidget(sidebarWidget, 1);

    mainContentWidget = new QWidget(this);
    mainContentWidget->setObjectName("MainContentWidget");
    QVBoxLayout *mainLayout = new QVBoxLayout(mainContentWidget);
    mainLayout->setContentsMargins(24, 16, 24, 20);
    mainLayout->setSpacing(16);

    QHBoxLayout *topBarLayout = new QHBoxLayout();
    topBarLayout->setSpacing(10);

    toggleSidebarButton = new QPushButton("≡", this);
    toggleSidebarButton->setObjectName("IconTextButton");
    topBarLayout->addWidget(toggleSidebarButton);

    modelComboBox = new QComboBox(this);
    modelComboBox->setObjectName("ModelComboBox");
    topBarLayout->addWidget(modelComboBox);

    QPushButton *refreshModelsBtn = new QPushButton("刷新模型", this);
    refreshModelsBtn->setObjectName("RefreshButton");
    refreshModelsBtn->setToolTip("点击重新加载本地模拟模型列表");
    topBarLayout->addWidget(refreshModelsBtn);
    connect(refreshModelsBtn, &QPushButton::clicked, this, &MainWindow::fetchModelList);

    topBarLayout->addStretch();
    mainLayout->addLayout(topBarLayout);

    chatDisplayEdit = new QTextEdit(this);
    chatDisplayEdit->setObjectName("ChatDisplayEdit");
    chatDisplayEdit->setReadOnly(true);
    chatDisplayEdit->append("<div style='color: #8e8ea0; margin-bottom: 12px;'><b>AIGateway 助手</b>: 你好！当前为<b>本地模拟模式</b>，可直接输入并测试界面与交互。</div>");
    mainLayout->addWidget(chatDisplayEdit, 1);

    QVBoxLayout *inputContainerLayout = new QVBoxLayout();
    inputContainerLayout->setContentsMargins(0, 0, 0, 0);
    inputContainerLayout->setSpacing(8);

    QHBoxLayout *inputBarLayout = new QHBoxLayout();
    inputBarLayout->setSpacing(10);

    inputLineEdit = new QLineEdit(this);
    inputLineEdit->setObjectName("InputLineEdit");
    inputLineEdit->setPlaceholderText("给 AIGateway 发送消息（支持 Enter 发送）...");
    inputBarLayout->addWidget(inputLineEdit);

    sendButton = new QPushButton("发送", this);
    sendButton->setObjectName("SendButton");
    inputBarLayout->addWidget(sendButton);

    inputContainerLayout->addLayout(inputBarLayout);

    QLabel *footerHintLabel = new QLabel("AIGateway 模型路由网关 [V0.3 本地模拟版]", this);
    footerHintLabel->setObjectName("FooterHintLabel");
    footerHintLabel->setAlignment(Qt::AlignCenter);
    inputContainerLayout->addWidget(footerHintLabel);

    mainLayout->addLayout(inputContainerLayout);
    rootLayout->addWidget(mainContentWidget, 4);
    setCentralWidget(centralWidget);

    connect(sendButton, &QPushButton::clicked, this, &MainWindow::onSendButtonClicked);
    connect(inputLineEdit, &QLineEdit::returnPressed, this, &MainWindow::onSendButtonClicked);
    connect(newChatButton, &QPushButton::clicked, this, &MainWindow::onNewChatClicked);
    connect(toggleSidebarButton, &QPushButton::clicked, this, &MainWindow::onToggleSidebar);
}

void MainWindow::applyModernStyle()
{
    QString modernQSS =
        "QMainWindow { background-color: #212121; color: #ececec; font-family: sans-serif; }"
        "QWidget#SidebarWidget { background-color: #171717; border-right: 1px solid #2f2f2f; }"
        "QPushButton#NewChatButton { background-color: transparent; color: #ececec; border: 1px solid #424242; border-radius: 6px; padding: 8px 12px; text-align: left; }"
        "QPushButton#NewChatButton:hover { background-color: #212121; }"
        "QLabel#SidebarTitleLabel { color: #8e8ea0; font-size: 11px; font-weight: bold; margin-top: 8px; }"
        "QListWidget#HistoryListWidget { background-color: transparent; border: none; color: #c5c5c5; }"
        "QListWidget#HistoryListWidget::item { padding: 8px; border-radius: 6px; }"
        "QListWidget#HistoryListWidget::item:hover { background-color: #212121; }"
        "QWidget#MainContentWidget { background-color: #212121; }"
        "QPushButton#IconTextButton { background-color: transparent; color: #b4b4b4; border: none; font-size: 16px; font-weight: bold; }"
        "QComboBox#ModelComboBox { background-color: #2f2f2f; color: #ececec; border: 1px solid #3f3f3f; border-radius: 6px; padding: 4px 12px; min-width: 180px; }"
        "QComboBox#ModelComboBox QAbstractItemView { background-color: #2f2f2f; color: #ececec; }"
        "QPushButton#RefreshButton { background-color: #2f2f2f; color: #38bdf8; border: 1px solid #3f3f3f; border-radius: 6px; padding: 4px 10px; }"
        "QPushButton#RefreshButton:hover { background-color: #3f3f3f; }"
        "QTextEdit#ChatDisplayEdit { background-color: #212121; color: #d1d5db; border: none; font-size: 14px; padding: 10px; }"
        "QLineEdit#InputLineEdit { background-color: #2f2f2f; color: #ececec; border: 1px solid #424242; border-radius: 10px; padding: 12px; font-size: 14px; }"
        "QPushButton#SendButton { background-color: #ececec; color: #171717; border: none; border-radius: 10px; padding: 0px 20px; font-weight: bold; }"
        "QPushButton#SendButton:hover { background-color: #ffffff; }"
        "QPushButton#SendButton:disabled { background-color: #555555; color: #888888; }"
        "QLabel#FooterHintLabel { color: #6b7280; font-size: 11px; }";
    this->setStyleSheet(modernQSS);
}

void MainWindow::fetchModelList()
{
    modelComboBox->clear();
    modelComboBox->addItem("自动选择模型 (Auto)", "auto");
    modelComboBox->addItem("模拟代码模型 (mock_code)", "mock_code");
    modelComboBox->addItem("模拟数学模型 (mock_math)", "mock_math");
    modelComboBox->addItem("模拟翻译模型 (mock_translation)", "mock_translation");

    chatDisplayEdit->append("<div style='color: #38bdf8; font-size: 12px; margin: 4px 0;'>系统: 已加载本地模拟模型列表。</div>");
}

void MainWindow::onModelsReply(QNetworkReply *reply)
{
    reply->deleteLater();
}

void MainWindow::setSendingState(bool sending)
{
    sendButton->setEnabled(!sending);
    inputLineEdit->setEnabled(!sending);
    modelComboBox->setEnabled(!sending);
    if (sending) {
        sendButton->setText("正在回答...");
    } else {
        sendButton->setText("发送");
        inputLineEdit->setFocus();
    }
}

void MainWindow::onSendButtonClicked()
{
    QString userText = inputLineEdit->text().trimmed();
    if (userText.isEmpty()) {
        chatDisplayEdit->append("<div style='color: #f87171; margin: 4px 0;'>提示: 发送内容不能为空！</div>");
        return;
    }

    chatDisplayEdit->append(QString("<div style='margin-top: 12px; margin-bottom: 8px;'><b style='color: #ffffff;'>我</b><br><span style='color: #ececec;'>%1</span></div>")
                                .arg(userText.toHtmlEscaped()));
    inputLineEdit->clear();

    setSendingState(true);
    timeoutTimer->start(130000);
    chatDisplayEdit->append("<div style='color: #8e8ea0; font-size: 12px; margin: 4px 0;'>系统: 正在本地模拟处理请求...</div>");

    QString selectedData = modelComboBox->currentData().toString();
    QString userMessageCopy = userText;

    QTimer::singleShot(800, this, [=]() {
        timeoutTimer->stop();
        setSendingState(false);

        QJsonObject jsonObj;
        jsonObj["answer"] = "这是针对 [ " + userMessageCopy + " ] 的本地模拟回答：已成功接收并完成处理。";

        if (selectedData == "auto") {
            jsonObj["selected_model"] = "mock_code";
            jsonObj["task"] = "code";
            jsonObj["reason"] = "本地自动路由：检测到通用指令，分发至 mock_code 模型";
            jsonObj["fallback_used"] = false;
        } else {
            jsonObj["selected_model"] = selectedData;
            jsonObj["task"] = "general";
            jsonObj["reason"] = "用户手动指定模型：" + selectedData;
            jsonObj["fallback_used"] = false;
        }

        jsonObj["request_id"] = "req-local-mock-9988";
        jsonObj["is_mock"] = true;

        QString task = jsonObj["task"].toString();
        if (task == "code") task = "编程";
        else if (task == "math") task = "数学";
        else if (task == "translation") task = "翻译";
        else task = "通用";

        QString selectedModel = jsonObj["selected_model"].toString();
        QString reason = jsonObj["reason"].toString();
        QString requestId = jsonObj["request_id"].toString();
        bool fallbackUsed = jsonObj["fallback_used"].toBool(false);
        bool isMock = jsonObj["is_mock"].toBool(true);

        QString mockTag = isMock ? "<span style='color: #f59e0b;'>[模拟]</span>" : "<span style='color: #10b981;'>[真实]</span>";
        QString fallbackHtml = fallbackUsed ? "<div style='color: #f59e0b; margin-top: 4px; font-size: 13px;'>⚠️ <b>已切换备用模型</b></div>" : "";

        QString detailsHtml = QString(
                                  "<div style='margin-top: 8px; margin-bottom: 12px; background-color: #2f2f2f; padding: 12px; border-radius: 8px;'>"
                                  "<b style='color: #38bdf8;'>AIGateway 响应 %1</b><br>"
                                  "<div style='margin: 6px 0; color: #e5e7eb;'>%2</div>"
                                  "<hr style='border: none; border-top: 1px solid #3f3f3f; margin: 8px 0;'>"
                                  "<div style='font-size: 12px; color: #9ca3af; line-height: 1.5;'>"
                                  "• <b>最终模型</b>: %3<br>"
                                  "• <b>问题类型</b>: %4<br>"
                                  "%5"
                                  "%6"
                                  "• <b>请求编号</b>: %7"
                                  "</div></div>"
                                  ).arg(mockTag)
                                  .arg(jsonObj["answer"].toString().toHtmlEscaped())
                                  .arg(selectedModel)
                                  .arg(task)
                                  .arg(reason.isEmpty() ? "" : QString("• <b>切换/选择理由</b>: " + reason.toHtmlEscaped() + "<br>"))
                                  .arg(fallbackHtml)
                                  .arg(requestId);

        chatDisplayEdit->append(detailsHtml);
    });
}

void MainWindow::onRequestTimeout()
{
    timeoutTimer->stop();
    setSendingState(false);
    chatDisplayEdit->append("<div style='color: #f87171; margin: 8px 0;'>错误: 请求超时！</div>");
}

void MainWindow::onNetworkReply(QNetworkReply *reply)
{
    reply->deleteLater();
}

void MainWindow::onNewChatClicked()
{
    chatDisplayEdit->clear();
    chatDisplayEdit->append("<div style='color: #8e8ea0; margin-bottom: 12px;'><b>AIGateway 助手</b>: 已开启新对话（本地模拟模式）。</div>");
}

void MainWindow::onToggleSidebar()
{
    isSidebarVisible = !isSidebarVisible;
    sidebarWidget->setVisible(isSidebarVisible);
}