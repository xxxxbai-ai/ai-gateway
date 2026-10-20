#include "mainwindow.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QJsonObject>
#include <QJsonDocument>
#include <QNetworkRequest>
#include <QUrl>
#include <QListWidget>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), isSidebarVisible(true)
{
    setWindowTitle("AIGateway 客户端");
    resize(900, 600);

    // 初始化网络管理器
    networkManager = new QNetworkAccessManager(this);
    connect(networkManager, &QNetworkAccessManager::finished, this, &MainWindow::onNetworkReply);

    setupUI();
    applyModernStyle();
}

MainWindow::~MainWindow() {}

void MainWindow::setupUI()
{
    centralWidget = new QWidget(this);
    QHBoxLayout *rootLayout = new QHBoxLayout(centralWidget);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // 左侧边栏
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

    // 右侧主内容区
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
    modelComboBox->addItem("mock_code (编程)", "mock_code");
    modelComboBox->addItem("mock_math (数学)", "mock_math");
    modelComboBox->addItem("mock (通用)", "mock");
    topBarLayout->addWidget(modelComboBox);

    topBarLayout->addStretch();
    mainLayout->addLayout(topBarLayout);

    chatDisplayEdit = new QTextEdit(this);
    chatDisplayEdit->setObjectName("ChatDisplayEdit");
    chatDisplayEdit->setReadOnly(true);
    chatDisplayEdit->append("<div style='color: #8e8ea0; margin-bottom: 12px;'><b>AIGateway 助手</b>: 你好！今天我可以帮你做些什么？</div>");
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

    QLabel *footerHintLabel = new QLabel("AIGateway 模型路由网关", this);
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
    QString modernQSS = R"(
        QMainWindow { background-color: #212121; color: #ececec; font-family: sans-serif; }
        QWidget#SidebarWidget { background-color: #171717; border-right: 1px solid #2f2f2f; }
        QPushButton#NewChatButton { background-color: transparent; color: #ececec; border: 1px solid #424242; border-radius: 6px; padding: 8px 12px; text-align: left; }
        QPushButton#NewChatButton:hover { background-color: #212121; }
        QLabel#SidebarTitleLabel { color: #8e8ea0; font-size: 11px; font-weight: bold; margin-top: 8px; }
        QListWidget#HistoryListWidget { background-color: transparent; border: none; color: #c5c5c5; }
        QListWidget#HistoryListWidget::item { padding: 8px; border-radius: 6px; }
        QListWidget#HistoryListWidget::item:hover { background-color: #212121; }
        QWidget#MainContentWidget { background-color: #212121; }
        QPushButton#IconTextButton { background-color: transparent; color: #b4b4b4; border: none; font-size: 16px; font-weight: bold; }
        QComboBox#ModelComboBox { background-color: #2f2f2f; color: #ececec; border: 1px solid #3f3f3f; border-radius: 6px; padding: 4px 12px; min-width: 180px; }
        QComboBox#ModelComboBox QAbstractItemView { background-color: #2f2f2f; color: #ececec; }
        QTextEdit#ChatDisplayEdit { background-color: #212121; color: #d1d5db; border: none; font-size: 14px; padding: 10px; }
        QLineEdit#InputLineEdit { background-color: #2f2f2f; color: #ececec; border: 1px solid #424242; border-radius: 10px; padding: 12px; font-size: 14px; }
        QPushButton#SendButton { background-color: #ececec; color: #171717; border: none; border-radius: 10px; padding: 0px 20px; font-weight: bold; }
        QPushButton#SendButton:hover { background-color: #ffffff; }
        QLabel#FooterHintLabel { color: #6b7280; font-size: 11px; }
    )";
    this->setStyleSheet(modernQSS);
}

void MainWindow::onSendButtonClicked()
{
    QString userText = inputLineEdit->text().trimmed();
    if (userText.isEmpty()) return;

    // 1. 显示用户消息（修复了 arg 占位符顺序：确保 %1 是用户文本，%2 是模型名称）
    chatDisplayEdit->append(QString("<div style='margin-top: 12px; margin-bottom: 8px;'><b style='color: #ffffff;'>我</b><br><span style='color: #ececec;'>%1</span></div>")
                                .arg(userText.toHtmlEscaped()));
    inputLineEdit->clear();

    // 2. 构造请求发送到网关 http://127.0.0.1:8080/api/chat
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
    jsonObj["message"] = userText; // 对应网关接收的 message 字段

    QJsonDocument doc(jsonObj);
    networkManager->post(request, doc.toJson());

    chatDisplayEdit->append("<div style='color: #8e8ea0; font-size: 12px; margin: 4px 0;'>系统: 正在请求网关...</div>");
}

void MainWindow::onNetworkReply(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseBytes = reply->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(responseBytes);
        QJsonObject jsonObj = jsonDoc.object();

        QString answer = jsonObj["answer"].toString();
        QString errorMsg = jsonObj["error"].toString();
        QString modelUsed = jsonObj["model"].toString(); // 实际使用的模型

        if (!errorMsg.isEmpty()) {
            chatDisplayEdit->append(QString("<div style='color: #f87171; margin: 8px 0;'>网关返回错误: %1</div>").arg(errorMsg.toHtmlEscaped()));
        } else {
            if (answer.isEmpty()) answer = QString::fromUtf8(responseBytes);

            // 修复 arg 格式问题：传入 %1 为模型，%2 为回答内容
            chatDisplayEdit->append(QString("<div style='margin-top: 8px; margin-bottom: 12px; background-color: #2f2f2f; padding: 12px; border-radius: 8px;'><b style='color: #38bdf8;'>AIGateway [%1]</b><br><span style='color: #e5e7eb;'>%2</span></div>")
                                        .arg(modelUsed.isEmpty() ? "mock" : modelUsed)
                                        .arg(answer.toHtmlEscaped()));
        }
    } else {
        // 连接不上网关时的提示
        chatDisplayEdit->append("<div style='color: #f87171; margin: 8px 0;'>错误: 无法连接到网关，请检查网关是否已启动！</div>");
    }

    reply->deleteLater();
}

void MainWindow::onNewChatClicked()
{
    chatDisplayEdit->clear();
    chatDisplayEdit->append("<div style='color: #8e8ea0; margin-bottom: 12px;'><b>AIGateway 助手</b>: 已开启新对话，请问今天有什么我可以帮您的？</div>");
}

void MainWindow::onToggleSidebar()
{
    isSidebarVisible = !isSidebarVisible;
    sidebarWidget->setVisible(isSidebarVisible);
}