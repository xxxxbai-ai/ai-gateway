#include "mainwindow.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QDateTime>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), isSidebarVisible(true)
{
    setWindowTitle("AIGateway 客户端");
    resize(900, 600);

    setupUI();
    applyModernStyle();
}

MainWindow::~MainWindow() {}

void MainWindow::setupUI()
{
    // 根中心部件
    centralWidget = new QWidget(this);
    QHBoxLayout *rootLayout = new QHBoxLayout(centralWidget);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // ==================== 1. 左侧边栏 ====================
    sidebarWidget = new QWidget(this);
    sidebarWidget->setObjectName("SidebarWidget");
    QVBoxLayout *sidebarLayout = new QVBoxLayout(sidebarWidget);
    sidebarLayout->setContentsMargins(12, 16, 12, 16);
    sidebarLayout->setSpacing(12);

    // 新建对话按钮
    newChatButton = new QPushButton("+ 新建对话", this);
    newChatButton->setObjectName("NewChatButton");
    sidebarLayout->addWidget(newChatButton);

    // 历史会话列表
    QLabel *historyLabel = new QLabel("历史会话", this);
    historyLabel->setObjectName("SidebarTitleLabel");
    sidebarLayout->addWidget(historyLabel);

    historyListWidget = new QListWidget(this);
    historyListWidget->setObjectName("HistoryListWidget");
    historyListWidget->addItem("默认 AI 对话会话");
    sidebarLayout->addWidget(historyListWidget);

    rootLayout->addWidget(sidebarWidget, 1); // 占用较小比例

    // ==================== 2. 右侧主内容区 ====================
    mainContentWidget = new QWidget(this);
    mainContentWidget->setObjectName("MainContentWidget");
    QVBoxLayout *mainLayout = new QVBoxLayout(mainContentWidget);
    mainLayout->setContentsMargins(24, 16, 24, 20);
    mainLayout->setSpacing(16);

    // 顶部栏：折叠按钮 + 模型选择器
    QHBoxLayout *topBarLayout = new QHBoxLayout();
    topBarLayout->setSpacing(10);

    toggleSidebarButton = new QPushButton("≡", this);
    toggleSidebarButton->setObjectName("IconTextButton");
    toggleSidebarButton->setToolTip("折叠/展开侧边栏");
    topBarLayout->addWidget(toggleSidebarButton);

    // 模型选择器
    modelComboBox = new QComboBox(this);
    modelComboBox->setObjectName("ModelComboBox");
    modelComboBox->addItem("自动选择模型 (Auto)");
    modelComboBox->addItem("mock_code (编程专家)");
    modelComboBox->addItem("mock_math (数学推理)");
    modelComboBox->addItem("mock (通用/翻译/写作)");
    topBarLayout->addWidget(modelComboBox);

    topBarLayout->addStretch(); // 靠左对齐
    mainLayout->addLayout(topBarLayout);

    // 聊天内容展示区（居中适中最大宽度感，采用现代富文本滚动）
    chatDisplayEdit = new QTextEdit(this);
    chatDisplayEdit->setObjectName("ChatDisplayEdit");
    chatDisplayEdit->setReadOnly(true);
    chatDisplayEdit->setPlaceholderText("在这里开始与 AIGateway 对话...");
    // 初始欢迎语
    chatDisplayEdit->append("<div style='color: #8e8ea0; margin-bottom: 12px;'><b>AIGateway 助手</b>: 你好！今天我可以帮你做些什么？</div>");
    mainLayout->addWidget(chatDisplayEdit, 1); // 占据主要伸缩空间

    // 底部输入控制区（输入框 + 发送按钮自然融合）
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

    // 底部小提示
    QLabel *footerHintLabel = new QLabel("AIGateway 模型路由网关 - 保持内容克制与高效", this);
    footerHintLabel->setObjectName("FooterHintLabel");
    footerHintLabel->setAlignment(Qt::AlignCenter);
    inputContainerLayout->addWidget(footerHintLabel);

    mainLayout->addLayout(inputContainerLayout);

    rootLayout->addWidget(mainContentWidget, 4); // 右侧占用较大比例
    setCentralWidget(centralWidget);

    // ==================== 3. 信号与槽绑定 ====================
    connect(sendButton, &QPushButton::clicked, this, &MainWindow::onSendButtonClicked);
    connect(inputLineEdit, &QLineEdit::returnPressed, this, &MainWindow::onSendButtonClicked);
    connect(newChatButton, &QPushButton::clicked, this, &MainWindow::onNewChatClicked);
    connect(toggleSidebarButton, &QPushButton::clicked, this, &MainWindow::onToggleSidebar);
}

void MainWindow::applyModernStyle()
{
    // 参考 ChatGPT 现代克制、低饱和度、以内容为中心的暗色主题 QSS 样式表
    QString modernQSS = R"(
        /* 全局窗口背景与字体 */
        QMainWindow {
            background-color: #212121;
            color: #ececec;
            font-family: "-apple-system", "Microsoft YaHei", "Segoe UI", sans-serif;
        }

        /* 左侧边栏 */
        QWidget#SidebarWidget {
            background-color: #171717;
            border-right: 1px solid #2f2f2f;
        }

        QPushButton#NewChatButton {
            background-color: transparent;
            color: #ececec;
            border: 1px solid #424242;
            border-radius: 6px;
            padding: 8px 12px;
            text-align: left;
            font-size: 13px;
        }
        QPushButton#NewChatButton:hover {
            background-color: #212121;
            border-color: #555555;
        }

        QLabel#SidebarTitleLabel {
            color: #8e8ea0;
            font-size: 11px;
            font-weight: bold;
            margin-top: 8px;
            padding-left: 4px;
        }

        QListWidget#HistoryListWidget {
            background-color: transparent;
            border: none;
            color: #c5c5c5;
            font-size: 13px;
        }
        QListWidget#HistoryListWidget::item {
            padding: 8px 10px;
            border-radius: 6px;
            margin-bottom: 2px;
        }
        QListWidget#HistoryListWidget::item:hover {
            background-color: #212121;
        }
        QListWidget#HistoryListWidget::item:selected {
            background-color: #2f2f2f;
            color: #ffffff;
        }

        /* 右侧主内容区 */
        QWidget#MainContentWidget {
            background-color: #212121;
        }

        /* 顶部栏按钮与模型选择器 */
        QPushButton#IconTextButton {
            background-color: transparent;
            color: #b4b4b4;
            border: none;
            font-size: 16px;
            font-weight: bold;
            padding: 4px 8px;
            border-radius: 4px;
        }
        QPushButton#IconTextButton:hover {
            background-color: #2f2f2f;
            color: #ffffff;
        }

        QComboBox#ModelComboBox {
            background-color: #2f2f2f;
            color: #ececec;
            border: 1px solid #3f3f3f;
            border-radius: 6px;
            padding: 4px 12px;
            font-size: 13px;
            min-width: 180px;
        }
        QComboBox#ModelComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 20px;
            border-left: none;
        }
        QComboBox#ModelComboBox QAbstractItemView {
            background-color: #2f2f2f;
            color: #ececec;
            selection-background-color: #424242;
            border: 1px solid #3f3f3f;
        }

        /* 聊天展示区域 */
        QTextEdit#ChatDisplayEdit {
            background-color: #212121;
            color: #d1d5db;
            border: none;
            font-size: 14px;
            line-height: 1.5;
            padding: 10px;
        }

        /* 底部输入框区 */
        QLineEdit#InputLineEdit {
            background-color: #2f2f2f;
            color: #ececec;
            border: 1px solid #424242;
            border-radius: 10px;
            padding: 12px 14px;
            font-size: 14px;
        }
        QLineEdit#InputLineEdit:focus {
            border: 1px solid #6b7280;
        }

        QPushButton#SendButton {
            background-color: #ececec;
            color: #171717;
            border: none;
            border-radius: 10px;
            padding: 0px 20px;
            font-weight: bold;
            font-size: 13px;
        }
        QPushButton#SendButton:hover {
            background-color: #ffffff;
        }
        QPushButton#SendButton:pressed {
            background-color: #d1d5db;
        }

        QLabel#FooterHintLabel {
            color: #6b7280;
            font-size: 11px;
            margin-top: 2px;
        }
    )";

    this->setStyleSheet(modernQSS);
}

void MainWindow::onSendButtonClicked()
{
    QString userText = inputLineEdit->text().trimmed();
    if (userText.isEmpty()) {
        return;
    }

    // 模拟排版：用户消息以右对齐或独立区块呈现，这里采用清晰的 ChatGPT 风格称呼展示
    chatDisplayEdit->append(QString("<div style='margin-top: 12px; margin-bottom: 8px;'><b style='color: #ffffff;'>我</b><br><span style='color: #ececec;'>%1</span></div>")
                                .arg(userText.toHtmlEscaped()));
    inputLineEdit->clear();

    // 模拟 AI 回答状态与返回结果
    QString selectedModel = modelComboBox->currentText();
    chatDisplayEdit->append(QString("<div style='margin-top: 8px; margin-bottom: 12px; background-color: #2f2f2f; padding: 12px; border-radius: 8px;'><b style='color: #38bdf8;'>AIGateway [%2]</b><br><span style='color: #e5e7eb;'>收到你的提问，正在通过路由模块调用大模型... (当前选择模式: 模拟响应正常)</span></div>")
                                .arg(userText.toHtmlEscaped())
                                .arg(selectedModel));
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