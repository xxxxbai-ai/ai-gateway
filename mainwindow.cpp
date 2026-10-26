#include "mainwindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSpinBox>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
QString escaped(QString text) {
    return text.toHtmlEscaped().replace('\n', "<br>");
}
QString taskName(const QString& task) {
    static const QMap<QString, QString> names{{"programming","编程"},{"math","数学"},
        {"translation","翻译"},{"writing","写作"},{"general","通用问答"}};
    return names.value(task, task);
}
}

MainWindow::MainWindow(QString gatewayUrl, QWidget* parent)
    : QMainWindow(parent), base_url_(std::move(gatewayUrl)), network_(new QNetworkAccessManager(this)) {
    setupUi();
    QTimer::singleShot(0, this, [this] {
        if (base_url_.isEmpty()) startGateway();
        else loadModels();
    });
}

MainWindow::~MainWindow() {
    if (gateway_ && gateway_->state() != QProcess::NotRunning) {
        gateway_->kill();
        gateway_->waitForFinished(1500);
    }
}

void MainWindow::setupUi() {
    setWindowTitle("AI 网关 · 模拟演示");
    resize(1080, 740);
    setMinimumSize(940, 640);
    auto* central = new QWidget(this);
    auto* root = new QHBoxLayout(central);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);
    auto* sidebar = new QFrame(central);
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(205);
    auto* side = new QVBoxLayout(sidebar);
    side->setContentsMargins(22,28,22,24);
    side->setSpacing(20);
    auto* logo = new QLabel("AI GATEWAY", sidebar);
    logo->setObjectName("brand");
    side->addWidget(logo);
    auto* badge = new QLabel("模拟演示版本", sidebar);
    badge->setObjectName("badge");
    side->addWidget(badge);
    auto* guide = new QLabel("试着这样演示\n\n"
        "1  输入不同题型\n观察自动分类和选模\n\n"
        "2  手动选择模型\n观察实际作答模型\n\n"
        "3  打开故障演示\n观察备用模型接手\n\n"
        "4  修改模型设置\n观察新问题的选模变化", sidebar);
    guide->setWordWrap(true);
    guide->setObjectName("guide");
    side->addWidget(guide);
    side->addStretch();
    clear_ = new QPushButton("清空聊天记录", sidebar);
    clear_->setObjectName("clearButton");
    side->addWidget(clear_);
    root->addWidget(sidebar);

    auto* content = new QWidget(central);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(28,24,28,22);
    layout->setSpacing(14);
    auto* heading = new QLabel("多模型智能路由网关", content);
    heading->setObjectName("heading");
    layout->addWidget(heading);
    auto* subtitle = new QLabel("输入问题，查看分类、选模理由和实际作答模型。", content);
    subtitle->setObjectName("subtitle");
    layout->addWidget(subtitle);

    auto* toolbar = new QHBoxLayout;
    model_box_ = new QComboBox(content);
    model_box_->setObjectName("modelComboBox");
    model_box_->setMinimumWidth(240);
    model_box_->addItem("自动选择模型", "auto");
    toolbar->addWidget(model_box_, 1);
    refresh_ = new QPushButton("重新连接", content);
    refresh_->setObjectName("refreshButton");
    settings_ = new QPushButton("模型设置", content);
    settings_->setObjectName("settingsButton");
    toolbar->addWidget(refresh_);
    toolbar->addWidget(settings_);
    layout->addLayout(toolbar);

    auto* options = new QHBoxLayout;
    failure_box_ = new QCheckBox("演示：让首选模型失败", content);
    failure_box_->setObjectName("failureCheckBox");
    failure_box_->setToolTip("只模拟首选模型明确拒绝；自动模式可以尝试一个备用，手动模式直接报错。");
    options->addWidget(failure_box_);
    options->addStretch();
    status_ = new QLabel("正在启动网关…", content);
    status_->setObjectName("statusLabel");
    status_->setMinimumWidth(220);
    status_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    options->addWidget(status_);
    layout->addLayout(options);

    messages_ = new QTextEdit(content);
    messages_->setObjectName("chatDisplay");
    messages_->setReadOnly(true);
    messages_->setPlaceholderText("这里显示回答、分类和选模理由");
    layout->addWidget(messages_, 1);

    auto* inputRow = new QHBoxLayout;
    input_ = new QLineEdit(content);
    input_->setObjectName("messageInput");
    input_->setPlaceholderText("例如：解释 C++ 指针，或把你好翻译成英文");
    input_->setMinimumHeight(46);
    send_ = new QPushButton("发送问题", content);
    send_->setObjectName("sendButton");
    send_->setMinimumHeight(46);
    send_->setMinimumWidth(110);
    inputRow->addWidget(input_, 1);
    inputRow->addWidget(send_);
    layout->addLayout(inputRow);
    auto* footer = new QLabel("模拟回答用于验证程序流程；每次只处理当前问题。", content);
    footer->setObjectName("footer");
    layout->addWidget(footer);
    root->addWidget(content, 1);
    setCentralWidget(central);

    setStyleSheet(R"(
        QWidget { font-family: "Microsoft YaHei UI"; font-size: 12px; color: #e5eaf3; }
        QMainWindow, QWidget { background-color: #111827; }
        QFrame#sidebar { background-color: #0b1220; border-right: 1px solid #263247; }
        QFrame#sidebar QLabel { background: transparent; }
        QLabel#brand { color: #67e8f9; font-size: 18px; font-weight: bold; }
        QLabel#badge { color: #a5b4fc; font-size: 12px; }
        QLabel#guide { color: #97a8c0; line-height: 1.5; }
        QLabel#heading { font-size: 25px; font-weight: bold; }
        QLabel#subtitle, QLabel#footer { color: #91a1b9; }
        QLabel#statusLabel { color: #a5b4c9; }
        QPushButton { background: #243249; border: 1px solid #33455f; border-radius: 7px; padding: 9px 14px; }
        QPushButton:hover { background: #32445e; }
        QPushButton:disabled { color: #68758a; background: #1c2637; border-color: #293548; }
        QPushButton#sendButton { color: #052331; background: #67e8f9; border-color: #67e8f9; font-weight: bold; }
        QPushButton#sendButton:hover { background: #a5f3fc; }
        QPushButton#sendButton:disabled { color: #60798a; background: #243b49; border-color: #243b49; }
        QTextEdit#chatDisplay { background: #172236; border: 1px solid #293b54; border-radius: 10px; padding: 14px; font-size: 14px; }
        QLineEdit, QComboBox, QSpinBox { background: #172236; border: 1px solid #354761; border-radius: 7px; padding: 7px; }
        QLineEdit:focus, QComboBox:focus { border-color: #67e8f9; }
        QComboBox QAbstractItemView { background: #172236; color: #e5eaf3; selection-background-color: #304965; }
        QCheckBox { color: #b8c8dd; spacing: 7px; }
        QDialog, QTableWidget { background: #111827; }
        QHeaderView::section { background: #243249; padding: 8px; border: 0; color: #dce5f4; }
        QTableWidget { gridline-color: #293b54; border: 1px solid #33455f; }
        QScrollBar:vertical { background: #172236; width: 9px; }
        QScrollBar::handle:vertical { background: #405571; min-height: 24px; border-radius: 4px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    )");
    connect(send_, &QPushButton::clicked, this, &MainWindow::sendQuestion);
    connect(input_, &QLineEdit::returnPressed, this, &MainWindow::sendQuestion);
    connect(refresh_, &QPushButton::clicked, this, [this] {
        if (base_url_.isEmpty()) startGateway(); else loadModels();
    });
    connect(settings_, &QPushButton::clicked, this, &MainWindow::openSettings);
    connect(clear_, &QPushButton::clicked, this, &MainWindow::clearMessages);
    setConnected(false, "正在启动网关…");
    clearMessages();
}

void MainWindow::setConnected(bool connected, const QString& message) {
    connected_ = connected;
    status_->setText(message);
    status_->setStyleSheet(connected ? "color:#86efac;" : "color:#fbbf24;");
    setBusy(busy_);
}

void MainWindow::setBusy(bool busy) {
    busy_ = busy;
    send_->setEnabled(connected_ && !busy);
    send_->setText(busy ? "正在回答…" : "发送问题");
    model_box_->setEnabled(connected_ && !busy);
    settings_->setEnabled(connected_ && !busy);
    refresh_->setEnabled(!busy);
    clear_->setEnabled(!busy);
    failure_box_->setEnabled(connected_ && demo_mode_ && !busy);
}

void MainWindow::startGateway() {
    if (gateway_ && gateway_->state() != QProcess::NotRunning) return;
    const QString directory = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
    const QString executable = directory + "/gateway.exe";
#else
    const QString executable = directory + "/gateway";
#endif
    if (!QFileInfo::exists(executable)) {
        setConnected(false, "找不到网关程序");
        showError("请把窗口程序和 gateway 程序放在同一个文件夹，再重新打开。");
        return;
    }
    if (!gateway_) {
        gateway_ = new QProcess(this);
        gateway_->setProcessChannelMode(QProcess::MergedChannels);
#ifdef Q_OS_WIN
        gateway_->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* arguments) {
            arguments->flags |= CREATE_NO_WINDOW;
        });
#endif
        connect(gateway_, &QProcess::readyReadStandardOutput, this, [this] {
            startup_output_ += gateway_->readAllStandardOutput();
            const auto text = QString::fromUtf8(startup_output_);
            const auto match = QRegularExpression("Gateway ready: (http://127\\.0\\.0\\.1:[0-9]+)").match(text);
            if (base_url_.isEmpty() && match.hasMatch()) {
                base_url_ = match.captured(1);
                loadModels();
            }
        });
        connect(gateway_, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
            base_url_.clear();
            setConnected(false, "网关已停止，可点击重新连接");
            if (!startup_output_.contains("Gateway ready:"))
                showError("网关启动失败：" + QString::fromUtf8(startup_output_).trimmed());
        });
        connect(gateway_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
                setConnected(false, "网关启动失败");
                showError("无法启动网关，请检查程序文件是否完整。");
            }
        });
    }
    startup_output_.clear();
    base_url_.clear();
    setConnected(false, "正在启动网关…");
    gateway_->setWorkingDirectory(directory);
    gateway_->start(executable, {"--demo", "--port", "0", "--config",
        directory + "/config/models.json", "--log", directory + "/logs/requests.jsonl"});
    QTimer::singleShot(12000, gateway_, [this] {
        if (gateway_->state() != QProcess::NotRunning && base_url_.isEmpty()) {
            gateway_->kill();
            showError("网关启动时间过长，请点击重新连接。");
        }
    });
}

void MainWindow::request(const QByteArray& method, const QString& path, const QJsonObject& body, Handler handler) {
    QNetworkRequest operation(QUrl(base_url_ + path));
    operation.setHeader(QNetworkRequest::ContentTypeHeader, "application/json; charset=utf-8");
    operation.setTransferTimeout(130000);
    auto* reply = method == "GET" ? network_->get(operation) :
        network_->post(operation, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QTimer::singleShot(130000, reply, [reply] {
        if (!reply->isFinished()) { reply->setProperty("deadlineExpired", true); reply->abort(); }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, handler = std::move(handler)] {
        const QByteArray bytes = reply->readAll();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QJsonParseError parse;
        const auto document = QJsonDocument::fromJson(bytes, &parse);
        const auto object = document.object();
        bool ok = status >= 200 && status < 300 && parse.error == QJsonParseError::NoError &&
            document.isObject() && object.value("success").isBool() && object.value("success").toBool();
        QString message;
        if (status == 0) {
            message = reply->property("deadlineExpired").toBool() ? "等待回答超时，请重试。" :
                "无法连接到网关，请点击重新连接再试。";
            setConnected(false, "连接已中断");
        } else if (parse.error != QJsonParseError::NoError || !document.isObject())
            message = "网关返回的内容格式不正确。";
        else if (!ok)
            message = object.value("error").toString(QString("请求未成功（HTTP %1）").arg(status));
        reply->deleteLater();
        handler(ok, object, message);
    });
}

void MainWindow::loadModels() {
    setConnected(false, "正在读取模型…");
    request("GET", "/api/models", {}, [this](bool ok, const QJsonObject& object, const QString& message) {
        if (!ok || !object.value("models").isArray()) {
            setConnected(false, "连接未就绪");
            showError(message.isEmpty() ? "无法取得模型列表。" : message);
            emit modelsLoaded(false);
            return;
        }
        const QString previous = model_box_->currentData().toString();
        model_box_->clear();
        model_box_->addItem("自动选择模型（按问题类型）", "auto");
        model_names_.clear();
        for (const auto value : object.value("models").toArray()) {
            const auto model = value.toObject();
            const auto id = model.value("id").toString();
            if (id.isEmpty()) continue;
            const auto name = model.value("name").toString(id);
            model_names_[id] = name;
            model_box_->addItem(name + (model.value("is_mock").toBool() ? " · 模拟" : " · 真实"), id);
        }
        const int index = model_box_->findData(previous);
        if (index >= 0) model_box_->setCurrentIndex(index);
        demo_mode_ = object.value("demo_mode").toBool(false);
        if (!demo_mode_) failure_box_->setChecked(false);
        setConnected(true, QString("已连接 · %1 个模型").arg(model_names_.size()));
        emit modelsLoaded(true);
    });
}

QString MainWindow::modelName(const QString& id) const { return model_names_.value(id, id); }

void MainWindow::appendCard(const QString& title, const QString& text, const QString& details) {
    messages_->append(QString("<div style='margin:8px 0;'><b style='color:#67e8f9;'>%1</b><br>"
        "<span style='color:#eef3fb;'>%2</span>%3</div><br>")
        .arg(escaped(title), escaped(text), details.isEmpty() ? "" :
            "<br><span style='color:#a5b7d0;font-size:11px;'>" + escaped(details) + "</span>"));
    messages_->verticalScrollBar()->setValue(messages_->verticalScrollBar()->maximum());
}

void MainWindow::showError(const QString& message) {
    messages_->append("<div style='margin:8px 0;color:#fca5a5;'><b>提示：</b>" + escaped(message) + "</div>");
}

void MainWindow::sendQuestion() {
    if (busy_ || !connected_) return;
    const QString text = input_->text().trimmed();
    if (text.isEmpty()) { showError("请输入问题后再发送。"); return; }
    const QString chosen = model_box_->currentData().toString();
    QJsonObject body{{"mode", chosen == "auto" ? "auto" : "manual"}, {"message", text},
        {"model", chosen == "auto" ? QJsonValue(QJsonValue::Null) : QJsonValue(chosen)},
        {"demo_failure", failure_box_->isChecked()}};
    appendCard("我", text);
    input_->clear();
    setBusy(true);
    status_->setText("正在处理问题…");
    request("POST", "/api/chat", body, [this](bool ok, const QJsonObject& object, const QString& error) {
        if (ok && (!object.value("answer").isString() || !object.value("selected_model").isString() ||
                   !object.value("task").isString() || !object.value("is_mock").isBool())) {
            ok = false;
            showError("回答缺少必要信息，请检查网关版本。");
        } else if (!ok) showError(error);
        if (ok) {
            const auto id = object.value("selected_model").toString();
            const bool switched = object.value("fallback_used").toBool();
            const QString title = (object.value("is_mock").toBool() ? "模拟回答 · " : "真实回答 · ") +
                modelName(id) + " [" + id + "]";
            const QString details = "题型：" + taskName(object.value("task").toString()) +
                (switched ? " · 已切换备用模型" : "") +
                (object.value("uncertain").toBool() ? " · 分类不确定，按通用处理" : "") +
                "\n选择理由：" + object.value("reason").toString() +
                "\n请求编号：" + object.value("request_id").toString();
            appendCard(title, object.value("answer").toString(), details);
            status_->setText(switched ? "完成 · 已切换备用模型" : "完成 · " + modelName(id));
        } else if (connected_) status_->setText("本次请求未成功，可修改后重试");
        setBusy(false);
        emit chatFinished(ok, object);
    });
}

void MainWindow::clearMessages() {
    messages_->clear();
    input_->clear();
    appendCard("开始演示", "试着输入编程、数学、翻译或写作问题。回答下方会显示题型和选择理由。",
        "打开故障演示后：自动模式可以尝试备用模型；手动模式直接显示失败。");
}

void MainWindow::openSettings() {
    settings_->setEnabled(false);
    request("GET", "/api/config", {}, [this](bool ok, const QJsonObject& object, const QString& error) {
        setBusy(busy_);
        if (!ok) { showError(error); return; }
        const QStringList tasks{"programming","math","translation","writing","general"};
        QDialog dialog(this);
        dialog.setObjectName("settingsDialog");
        dialog.setWindowTitle("模型设置 · 模拟演示");
        dialog.resize(850, 390);
        auto* layout = new QVBoxLayout(&dialog);
        auto* note = new QLabel("修改启用状态或能力分，点击保存后对下一个问题生效。分数为演示预设值。", &dialog);
        note->setWordWrap(true);
        layout->addWidget(note);
        auto* table = new QTableWidget(0, 7, &dialog);
        table->setObjectName("modelSettingsTable");
        table->setHorizontalHeaderLabels({"模型","启用","编程","数学","翻译","写作","通用"});
        table->verticalHeader()->hide();
        table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        for (int column = 1; column < 7; ++column) table->setColumnWidth(column, column == 1 ? 50 : 88);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        QStringList ids;
        for (const auto value : object.value("models").toArray()) {
            const auto model = value.toObject();
            if (!model.value("is_mock").toBool()) continue;
            const int row = table->rowCount();
            table->insertRow(row);
            table->setRowHeight(row, 48);
            const auto id = model.value("id").toString();
            ids.push_back(id);
            table->setItem(row, 0, new QTableWidgetItem(model.value("name").toString()));
            auto* enabled = new QCheckBox(table);
            enabled->setObjectName("enable_" + id);
            enabled->setChecked(model.value("enabled").toBool());
            table->setCellWidget(row, 1, enabled);
            const auto scores = model.value("scores").toObject();
            for (int column = 0; column < tasks.size(); ++column) {
                auto* score = new QSpinBox(table);
                score->setObjectName("score_" + id + "_" + tasks[column]);
                score->setRange(0,100);
                score->setValue(scores.value(tasks[column]).toInt());
                table->setCellWidget(row, column + 2, score);
            }
        }
        layout->addWidget(table);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
        buttons->button(QDialogButtonBox::Save)->setText("保存设置");
        buttons->button(QDialogButtonBox::Save)->setObjectName("saveSettingsButton");
        buttons->button(QDialogButtonBox::Cancel)->setText("取消");
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout->addWidget(buttons);
        if (dialog.exec() != QDialog::Accepted) return;
        QJsonArray changes;
        for (int row = 0; row < ids.size(); ++row) {
            QJsonObject scores;
            for (int column = 0; column < tasks.size(); ++column)
                scores[tasks[column]] = qobject_cast<QSpinBox*>(table->cellWidget(row, column + 2))->value();
            changes.append(QJsonObject{{"id", ids[row]}, {"enabled",
                qobject_cast<QCheckBox*>(table->cellWidget(row, 1))->isChecked()}, {"scores", scores}});
        }
        setBusy(true);
        request("POST", "/api/config", {{"models", changes}},
            [this](bool saved, const QJsonObject&, const QString& message) {
                setBusy(false);
                if (!saved) showError(message);
                else { appendCard("模型设置", "已保存，新的问题会按新设置选模型。"); loadModels(); }
            });
    });
}
