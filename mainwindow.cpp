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
    setWindowTitle("AIGateway 客户端 v0.3 [真实联调版]");
    resize(950, 650);

    // 初始化网络管理器
    networkManager = new QNetworkAccessManager(this);
    connect(networkManager, &QNetworkAccessManager::finished, this, &MainWindow::onNetworkReply);

    modelManager = new QNetworkAccessManager(this);
    connect(modelManager, &QNetworkAccessManager::finished, this, &MainWindow::onModelsReply);

    // 初始化 130 秒超时定时器
    timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);
    connect(timeoutTimer, &QTimer::timeout, this, &MainWindow::onRequestTimeout);

    setupUI();
    applyModernStyle();

    // 启动时自动向真实网关请求模型列表
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
    historyListWidget->addItem("默认 AI 对话会话");
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
    modelComboBox->addItem("自动选择模型 (Auto)", "auto");
    topBarLayout->addWidget(modelComboBox);

    QPushButton *refreshModelsBtn = new QPushButton("刷新模型", this);
    refreshModelsBtn->setObjectName("RefreshButton");
    refreshModelsBtn->setToolTip("点击重新从网关获取模型列表");
    topBarLayout->addWidget(refreshModelsBtn);
    connect(refreshModelsBtn, &QPushButton::clicked, this, &MainWindow::fetchModelList);

    topBarLayout->addStretch();
    mainLayout->addLayout(topBarLayout);

    chatDisplayEdit = new QTextEdit(this);
    chatDisplayEdit->setObjectName("ChatDisplayEdit");
    chatDisplayEdit->setReadOnly(true);
    chatDisplayEdit->append("<div style='color: #8e8ea0; margin-bottom: 12px;'><b>AIGateway 助手</b>: 客户端已就绪，正在连接本地网关 (127.0.0.1:8080)...</div>");
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

    QLabel *footerHintLabel = new QLabel("AIGateway 模型路由网关 [V0.3 真实联调版]", this);
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

// 真实请求网关获取模型列表 GET /api/models
void MainWindow::fetchModelList()
{
    QUrl url("http://127.0.0.1:8080/api/models");
    QNetworkRequest request(url);
    modelManager->get(request);
}

void MainWindow::onModelsReply(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        chatDisplayEdit->append("<div style='color: #f87171; margin: 4px 0;'>系统提示: 无法从网关加载模型列表，请确认网关已启动并点击“刷新模型”重试。</div>");
        reply->deleteLater();
        return;
    }

    QByteArray responseBytes = reply->readAll();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseBytes);

    QJsonArray modelsArray;
    if (jsonDoc.isArray()) {
        modelsArray = jsonDoc.array();
    } else if (jsonDoc.isObject()) {
        modelsArray = jsonDoc.object()["models"].toArray();
    }

    if (!modelsArray.isEmpty()) {
        QString currentData = modelComboBox->currentData().toString();

        modelComboBox->clear();
        modelComboBox->addItem("自动选择模型 (Auto)", "auto");

        for (const QJsonValue &val : modelsArray) {
            QJsonObject obj = val.toObject();
            QString id = obj["id"].toString();
            QString name = obj["name"].toString();
            if (id.isEmpty()) id = obj["model"].toString();
            if (name.isEmpty()) name = id;

            if (!id.isEmpty()) {
                modelComboBox->addItem(name, id);
            }
        }

        int index = modelComboBox->findData(currentData);
        if (index != -1) {
            modelComboBox->setCurrentIndex(index);
        }
        chatDisplayEdit->append("<div style='color: #38bdf8; font-size: 12px; margin: 4px 0;'>系统: 模型列表已成功从网关刷新。</div>");
    }

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
    timeoutTimer->start(130000); // 130秒总超时限制

    // 真实向网关发送 POST /api/chat 请求
    QUrl url("http://127.0.0.1:8080/api/chat");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject jsonObj;
    QString selectedData = modelComboBox->currentData().toString();
    if (selectedData == "auto") {
        jsonObj["mode"] = "auto";
        jsonObj["model"] = QJsonValue::Null;
    } else {
        jsonObj["mode"] = "manual";
        jsonObj["model"] = selectedData;
    }
    jsonObj["message"] = userText;

    QJsonDocument doc(jsonObj);
    networkManager->post(request, doc.toJson());

    chatDisplayEdit->append("<div style='color: #8e8ea0; font-size: 12px; margin: 4px 0;'>系统: 正在等待网关响应...</div>");
}

void MainWindow::onRequestTimeout()
{
    timeoutTimer->stop();
    setSendingState(false);
    chatDisplayEdit->append("<div style='color: #f87171; margin: 8px 0;'>错误: 请求网关超时（超过130秒），请检查网关状态！</div>");
}

void MainWindow::onNetworkReply(QNetworkReply *reply)
{
    timeoutTimer->stop();
    setSendingState(false);

    QByteArray responseBytes = reply->readAll();
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    // 检查底层网络错误（如网关没开）
    if (reply->error() != QNetworkReply::NoError && statusCode == 0) {
        chatDisplayEdit->append("<div style='color: #f87171; margin: 8px 0;'>错误: 无法连接到网关，请检查网关是否已启动！</div>");
        reply->deleteLater();
        return;
    }

    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseBytes);
    QJsonObject jsonObj = jsonDoc.object();

    QString answer = jsonObj["answer"].toString();
    QString errorMsg = jsonObj["error"].toString();
    if (errorMsg.isEmpty()) {
        errorMsg = jsonObj["message"].toString();
    }

    // 处理 HTTP 错误或业务错误返回
    if (!errorMsg.isEmpty() || statusCode >= 400 || jsonDoc.isNull()) {
        if (errorMsg.isEmpty()) {
            errorMsg = QString::fromUtf8(responseBytes);
            if (errorMsg.isEmpty()) errorMsg = "返回数据格式不正确或为空";
        }
        chatDisplayEdit->append(QString("<div style='color: #f87171; margin: 8px 0;'>网关错误 (HTTP %1): %2</div>")
                                    .arg(statusCode == 0 ? 500 : statusCode)
                                    .arg(errorMsg.toHtmlEscaped()));
        reply->deleteLater();
        return;
    }

    // 解析详细返回字段
    QString task = jsonObj["task"].toString();
    if (task == "code") task = "编程";
    else if (task == "math") task = "数学";
    else if (task == "translation") task = "翻译";
    else if (task.isEmpty()) task = "通用";

    QString selectedModel = jsonObj["selected_model"].toString();
    if (selectedModel.isEmpty()) selectedModel = "unknown";

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
                              .arg(answer.isEmpty() ? QString::fromUtf8(responseBytes).toHtmlEscaped() : answer.toHtmlEscaped())
                              .arg(selectedModel)
                              .arg(task)
                              .arg(reason.isEmpty() ? "" : QString("• <b>切换/选择理由</b>: " + reason.toHtmlEscaped() + "<br>"))
                              .arg(fallbackHtml)
                              .arg(requestId);

    chatDisplayEdit->append(detailsHtml);
    reply->deleteLater();
}

void MainWindow::onNewChatClicked()
{
    chatDisplayEdit->clear();
    chatDisplayEdit->append("<div style='color: #8e8ea0; margin-bottom: 12px;'><b>AIGateway 助手</b>: 已开启新对话。</div>");
}

void MainWindow::onToggleSidebar()
{
    isSidebarVisible = !isSidebarVisible;
    sidebarWidget->setVisible(isSidebarVisible);
}