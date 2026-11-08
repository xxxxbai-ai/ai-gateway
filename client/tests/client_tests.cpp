#include "conversation.h"
#include "gateway_client.h"
#include "mainwindow.h"
#include "widgets.h"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QFontInfo>
#include <QImage>
#include <QInputMethodEvent>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBlock>
#include <QTextTable>
#include <QTextLayout>
#include <QTimer>
#include <QToolButton>
#include <memory>

using namespace client;
class HttpFixture : public QObject {
    Q_OBJECT
  public:
    QTcpServer server;
    struct Response {
        int status = 200;
        QByteArray body;
        int delay = 0;
    };
    QVector<Response> chatResponses;
    QVector<QJsonObject> requests;
    QByteArray models =
        R"({"success":true,"models":[{"id":"deepseek_flash","name":"DeepSeek V4.1 Flash","is_mock":false},{"id":"qwen_plus","name":"通义千问 3.7 Plus（北京）","is_mock":false},{"id":"kimi_pro","name":"Kimi K2.6","is_mock":false}]})";
    int modelDelay = 0;
    explicit HttpFixture(QObject *parent = nullptr) : QObject(parent) {
        if (!server.listen(QHostAddress::LocalHost, 0))
            qFatal("mock server failed");
        connect(&server, &QTcpServer::newConnection, this, [this] {
            while (server.hasPendingConnections()) {
                auto *socket = server.nextPendingConnection();
                auto bytes = std::make_shared<QByteArray>();
                connect(socket, &QIODevice::readyRead, this, [this, socket, bytes] {
                    bytes->append(socket->readAll());
                    const auto headerEnd = bytes->indexOf("\r\n\r\n");
                    if (headerEnd < 0 || socket->property("handled").toBool())
                        return;
                    qsizetype length = 0;
                    for (const auto &line : bytes->left(headerEnd).split('\n'))
                        if (line.toLower().startsWith("content-length:"))
                            length = line.mid(15).trimmed().toLongLong();
                    if (bytes->size() < headerEnd + 4 + length)
                        return;
                    socket->setProperty("handled", true);
                    Response response;
                    if (bytes->startsWith("GET /api/models")) {
                        response.body = models;
                        response.delay = modelDelay;
                    } else {
                        requests.append(QJsonDocument::fromJson(bytes->mid(headerEnd + 4, length)).object());
                        response =
                            chatResponses.isEmpty() ? Response{200, success(), 0} : chatResponses.takeFirst();
                    }
                    QTimer::singleShot(response.delay, socket, [socket, response] {
                        if (socket->state() != QAbstractSocket::ConnectedState)
                            return;
                        const auto header = "HTTP/1.1 " + QByteArray::number(response.status) +
                                            " Result\r\nContent-Type: application/json; "
                                            "charset=utf-8\r\nConnection: close\r\nContent-Length: " +
                                            QByteArray::number(response.body.size()) + "\r\n\r\n";
                        socket->write(header + response.body);
                        socket->disconnectFromHost();
                    });
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
    }
    QUrl url() const {
        return QUrl("http://127.0.0.1:" + QString::number(server.serverPort()) + "/");
    }
    static QByteArray success(const QString &model = "deepseek_flash", bool fallback = false) {
        return QJsonDocument(
                   QJsonObject{
                       {"success", true},
                       {"answer",
                        QStringLiteral(
                            "## "
                            "继承，让已有代码继续生长\n\n**继承**"
                            "允许一个类复用另一个类的成员，再增加自己的功能。\n\n### "
                            "一个小例子\n\n```cpp\nclass Animal {\npublic:\n    void eat() {}\n};\n\nclass "
                            "Dog : public Animal {\npublic:\n    void bark() {}\n};\n```\n\n- `Animal` "
                            "是基类。\n- `Dog` 是派生类，既能 `eat()`，也能 `bark()`。\n\n| 类 | 作用 |\n| "
                            "--- | --- |\n| Animal | 提供通用功能 |\n| Dog | 扩展专属功能 |\n\n> "
                            "适合描述“狗是一种动物”这样的关系。")},
                       {"selected_model", model},
                       {"request_id", "fixture-request-1"},
                       {"task", "programming"},
                       {"uncertain", false},
                       {"reason", "测试分类依据；手动指定模型 deepseek_flash"},
                       {"is_mock", false},
                       {"fallback_used", fallback}})
            .toJson();
    }
};
class ClientTests : public QObject {
    Q_OBJECT
  private slots:
    void inputMethodCompositionAndPlaceholder() {
        InputEditor editor;
        editor.setAttribute(Qt::WA_DontShowOnScreen);
        editor.resize(720, 68);
        editor.setCursorWidth(0);
        editor.show();
        QTest::qWait(30);
        const auto hint = editor.placeholderText();
        QVERIFY(!hint.isEmpty());
        const auto artifact = qEnvironmentVariable("AIGATEWAY_TEST_ARTIFACT_DIR");
        if (!artifact.isEmpty())
            QVERIFY(editor.grab().save(QDir(artifact).filePath("input_empty.png")));
        int step = 0;
        for (const auto &preedit : {QString("h"), QString("hai'you'zhe'ge'zi'ti'zen'me'chong'die'le"),
                                   QString("hai")}) {
            QTextCharFormat format;
            format.setFontUnderline(true);
            QInputMethodEvent event(preedit, {{QInputMethodEvent::TextFormat, 0, int(preedit.size()), format}});
            QApplication::sendEvent(&editor, &event);
            QVERIFY(editor.toPlainText().isEmpty());
            QCOMPARE(editor.document()->firstBlock().layout()->preeditAreaText(), preedit);
            const auto composed = editor.viewport()->grab().toImage();
            const auto currentHint = editor.placeholderText();
            editor.setPlaceholderText({});
            const auto withoutHint = editor.viewport()->grab().toImage();
            editor.setPlaceholderText(currentHint);
            if (!artifact.isEmpty()) {
                QVERIFY(composed.save(QDir(artifact).filePath(QString("input_preedit_%1.png").arg(step))));
                QVERIFY(withoutHint.save(QDir(artifact).filePath(QString("input_reference_%1.png").arg(step))));
            }
            QVERIFY2(composed == withoutHint, "Placeholder text must not overlap the IME preedit text");
            ++step;
        }
        QInputMethodEvent cancel;
        QApplication::sendEvent(&editor, &cancel);
        QVERIFY(editor.toPlainText().isEmpty());
        QCOMPARE(editor.placeholderText(), hint);
        QInputMethodEvent preedit("hai'you'zhe'ge", {});
        QApplication::sendEvent(&editor, &preedit);
        QInputMethodEvent commit;
        commit.setCommitString(QStringLiteral("还有这个"));
        QApplication::sendEvent(&editor, &commit);
        QCOMPARE(editor.toPlainText(), QStringLiteral("还有这个"));
        QCOMPARE(editor.placeholderText(), hint);
        if (!artifact.isEmpty())
            QVERIFY(editor.grab().save(QDir(artifact).filePath("input_committed.png")));
        editor.selectAll();
        QTest::keyClick(&editor, Qt::Key_Backspace);
        QVERIFY(editor.toPlainText().isEmpty());
        QCOMPARE(editor.placeholderText(), hint);
        if (!artifact.isEmpty())
            QVERIFY(editor.grab().save(QDir(artifact).filePath("input_cleared.png")));
        QSignalSpy submitted(&editor, &InputEditor::submitRequested);
        QInputMethodEvent composing("hai", {});
        QApplication::sendEvent(&editor, &composing);
        QTest::keyClick(&editor, Qt::Key_Return);
        QVERIFY(submitted.isEmpty());
        QInputMethodEvent finish;
        finish.setCommitString(QStringLiteral("还"));
        QApplication::sendEvent(&editor, &finish);
        QTest::keyClick(&editor, Qt::Key_Return);
        QCOMPARE(submitted.size(), 1);
    }
    void urlValidation() {
        QVERIFY(!GatewayClient::normalizeUrl("http://127.0.0.1:8080").isEmpty());
        QCOMPARE(GatewayClient::normalizeUrl("https://example.com/gateway").path(), QString("/gateway/"));
        QVERIFY(GatewayClient::normalizeUrl("http://example.com").isEmpty());
        QVERIFY(GatewayClient::normalizeUrl("https://key@example.com").isEmpty());
        QVERIFY(GatewayClient::normalizeUrl("https://example.com?token=abc").isEmpty());
        QCOMPARE(friendlyName("通义千问 3.7 Plus（北京）"), QString("通义千问 3.7 Plus"));
    }
    void persistentHistoryAndRecovery() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        ConversationStore store(directory.path());
        const auto id = store.create();
        auto *c = store.find(id);
        c->title = "中文历史";
        Message user;
        user.id = newId();
        user.role = "user";
        user.content = "<b>普通文本</b>\n第二行";
        c->messages.append(user);
        Message answer;
        answer.id = newId();
        answer.role = "assistant";
        answer.status = "pending";
        answer.replyTo = user.id;
        c->messages.append(answer);
        QVERIFY(store.save());
        ConversationStore reloaded(directory.path());
        QVERIFY(reloaded.load());
        QCOMPARE(reloaded.conversations.size(), 1);
        QCOMPARE(reloaded.conversations[0].title, QString("中文历史"));
        QCOMPARE(reloaded.conversations[0].messages[1].status, QString("cancelled"));
        QVERIFY(exportMarkdown(reloaded.conversations[0]).contains("第二行"));
        QFile broken(store.filePath());
        QVERIFY(broken.open(QIODevice::WriteOnly));
        broken.write("{broken");
        broken.close();
        ConversationStore recovery(directory.path());
        QString warning;
        QVERIFY(!recovery.load(&warning));
        QVERIFY(!warning.isEmpty());
        QVERIFY(!QDir(directory.path()).entryList({"history.json.unreadable-*"}).isEmpty());
        recovery.create();
        QVERIFY(recovery.save());
        QVERIFY(reloaded.load());
    }
    void markdownRendersAndBlocksResources() {
        MarkdownView view;
        view.setAttribute(Qt::WA_DontShowOnScreen);
        view.resize(760, 600);
        view.setAnswer(
            "# 标题\n\n**加粗**\n\n```cpp\nint main() {\n    return 0;\n}\n```\n\n| A | B |\n| --- | --- "
            "|\n| 1 | 2 |\n\n<script>alert(1)</script>\n![远程图片](https://example.com/a.png)");
        view.show();
        QTest::qWait(30);
        const auto plain = view.toPlainText();
        QVERIFY(plain.contains("标题"));
        QVERIFY(!plain.contains("**"));
        QVERIFY(plain.contains("    return 0;"));
        // HTML-like input may remain visible as literal text, but must never be executable markup.
        QVERIFY(!view.toHtml().contains("<script>"));
        QVERIFY(view.loadResource(QTextDocument::ImageResource, QUrl("file:///C:/Windows/win.ini"))
                    .canConvert<QImage>());
        bool table = false;
        for (auto *frame : view.document()->rootFrame()->childFrames())
            if (qobject_cast<QTextTable *>(frame))
                table = true;
        QVERIFY(table);
    }
    void networkModelsManualAndFallback() {
        HttpFixture server;
        GatewayClient api(server.url());
        QSignalSpy models(&api, &GatewayClient::modelsReady);
        api.fetchModels();
        QTRY_COMPARE(models.size(), 1);
        QCOMPARE(models.at(0).at(0).toJsonArray().size(), 3);
        QSignalSpy success(&api, &GatewayClient::chatSucceeded);
        QSignalSpy fail(&api, &GatewayClient::chatFailed);
        server.chatResponses.append({200, HttpFixture::success("kimi_pro"), 0});
        api.sendChat(11, {{"mode", "manual"}, {"model", "kimi_pro"}, {"message", "解释继承"}});
        QTRY_COMPARE(success.size(), 1);
        QVERIFY(fail.isEmpty());
        QCOMPARE(server.requests.last()["model"].toString(), QString("kimi_pro"));
        QCOMPARE(success.at(0).at(1).toJsonObject()["selected_model"].toString(), QString("kimi_pro"));
        server.chatResponses.append({200, HttpFixture::success("qwen_plus", true), 0});
        api.sendChat(12, {{"mode", "auto"}, {"model", QJsonValue::Null}, {"message", "解释继承"}});
        QTRY_COMPARE(success.size(), 2);
        QCOMPARE(success.last().at(1).toJsonObject()["selected_model"].toString(), QString("qwen_plus"));
        server.models = R"({"success":true,"models":[]})";
        api.fetchModels();
        QTRY_COMPARE(models.size(), 2);
        QVERIFY(models.last().at(0).toJsonArray().isEmpty());
    }
    void timeoutCancellationAndLateReplies() {
        HttpFixture server;
        GatewayClient api(server.url(), nullptr, 55, 55);
        QSignalSpy success(&api, &GatewayClient::chatSucceeded);
        QSignalSpy fail(&api, &GatewayClient::chatFailed);
        const QJsonObject request{{"mode", "auto"}, {"model", QJsonValue::Null}, {"message", "测试"}};
        server.chatResponses.append({200, HttpFixture::success(), 200});
        api.sendChat(21, request);
        QTRY_COMPARE(fail.size(), 1);
        QCOMPARE(fail.last().at(2).toJsonObject()["error_code"].toString(), QString("timeout"));
        server.chatResponses.append({200, HttpFixture::success(), 180});
        api.sendChat(22, request);
        QTRY_COMPARE(server.requests.size(), 2);
        api.cancelChat();
        QCOMPARE(fail.size(), 2);
        QCOMPARE(fail.last().at(2).toJsonObject()["error_code"].toString(), QString("cancelled"));
        api.sendChat(23, request);
        QTRY_COMPARE(success.size(), 1);
        QTest::qWait(230);
        QCOMPARE(success.size(), 1);
        QCOMPARE(fail.size(), 2);
        QVERIFY(!api.busy());
        server.modelDelay = 180;
        QSignalSpy modelFail(&api, &GatewayClient::modelsFailed);
        api.fetchModels();
        QTRY_COMPARE(modelFail.size(), 1);
        server.modelDelay = 0;
        QSignalSpy modelOk(&api, &GatewayClient::modelsReady);
        api.fetchModels();
        QTRY_COMPARE(modelOk.size(), 1);
        QTest::qWait(180);
        QCOMPARE(modelOk.size(), 1);
    }
    void rejectsMalformedAndPreservesServerError() {
        HttpFixture server;
        GatewayClient api(server.url());
        QSignalSpy fail(&api, &GatewayClient::chatFailed);
        const QJsonObject request{{"mode", "auto"}, {"model", QJsonValue::Null}, {"message", "测试"}};
        const QVector<QByteArray> bodies{
            "not json", R"({"success":true,"answer":"x"})",
            R"({"success":true,"answer":"","selected_model":"x","request_id":"r","is_mock":false})"};
        for (const auto &body : bodies) {
            server.chatResponses.append({200, body, 0});
            api.sendChat(30 + fail.size(), request);
            const auto before = fail.size();
            QTRY_COMPARE(fail.size(), before + 1);
            QCOMPARE(fail.last().at(2).toJsonObject()["error_code"].toString(), QString("invalid_response"));
        }
        server.chatResponses.append(
            {503,
             R"({"success":false,"error":"额度不足，请检查账号","error_code":"upstream_error","request_id":"err-1"})",
             0});
        api.sendChat(40, request);
        QTRY_COMPARE(fail.size(), 4);
        QVERIFY(fail.last().at(1).toString().contains("额度不足"));
        QCOMPARE(fail.last().at(2).toJsonObject()["request_id"].toString(), QString("err-1"));
    }
    void interfaceSessionOwnershipAndRestart() {
        HttpFixture server;
        QTemporaryDir directory;
        QString firstId;
        {
            MainWindow window(directory.path(), server.url());
            window.setAttribute(Qt::WA_DontShowOnScreen);
            window.resize(1360, 1000);
            window.show();
            auto *combo = window.findChild<QComboBox *>("modelSelector");
            auto *input = window.findChild<InputEditor *>("messageInput");
            auto *send = window.findChild<QPushButton *>("SendButton");
            QVERIFY(combo && input && send);
            QTRY_COMPARE(combo->count(), 4);
            QVERIFY(combo->findText("通义千问 3.7 Plus") >= 0);
            input->setPlainText("解释一下 C++ 的继承，给一个小例子。");
            firstId = window.currentConversationId();
            server.chatResponses.append({200, HttpFixture::success("qwen_plus", true), 180});
            QTest::mouseClick(send, Qt::LeftButton);
            QTRY_COMPARE(server.requests.size(), 1);
            auto *newChat = window.findChild<QPushButton *>("NewChat");
            QTest::mouseClick(newChat, Qt::LeftButton);
            const auto secondId = window.currentConversationId();
            QVERIFY(secondId != firstId);
            QTRY_COMPARE(window.history().conversations.size(), 2);
            QTRY_VERIFY([&] {
                for (const auto &c : window.history().conversations)
                    if (c.id == firstId)
                        return c.messages.last().status == "complete";
                return false;
            }());
            QCOMPARE(window.currentConversationId(), secondId);
            QVERIFY(window.findChildren<MarkdownView *>().isEmpty());
            auto *list = window.findChild<QListWidget *>("History");
            for (int i = 0; i < list->count(); ++i)
                if (list->item(i)->data(Qt::UserRole).toString() == firstId)
                    list->setCurrentRow(i);
            QTRY_COMPARE(window.currentConversationId(), firstId);
            QTRY_VERIFY(!window.findChildren<MarkdownView *>().isEmpty());
            bool fallbackName = false, realTag = false;
            for (auto *item : window.findChildren<QLabel *>()) {
                if (item->objectName() == "ModelBadge" && item->text() == "通义千问 3.7 Plus")
                    fallbackName = true;
                if (item->text().contains("[真实]") || item->text().contains("【真实】"))
                    realTag = true;
            }
            QVERIFY(fallbackName);
            QVERIFY(!realTag);
            const auto artifact = qEnvironmentVariable("AIGATEWAY_TEST_ARTIFACT_DIR");
            if (!artifact.isEmpty()) {
                QDir().mkpath(artifact);
                QTest::qWait(150);
                QVERIFY(window.grab().save(QDir(artifact).filePath("客户端_回答排版.png")));
            }
            window.close();
        }
        MainWindow reopened(directory.path(), server.url());
        reopened.setAttribute(Qt::WA_DontShowOnScreen);
        reopened.show();
        QTRY_COMPARE(reopened.findChild<QComboBox *>("modelSelector")->count(), 4);
        QCOMPARE(reopened.currentConversationId(), firstId);
        QCOMPARE(reopened.history().conversations.size(), 2);
        QVERIFY(!reopened.findChildren<MarkdownView *>().isEmpty());
    }
    void interfaceKeyboardAndOfflineRecovery() {
        HttpFixture server;
        QTemporaryDir directory;
        MainWindow window(directory.path(), server.url());
        window.setAttribute(Qt::WA_DontShowOnScreen);
        window.show();
        auto *combo = window.findChild<QComboBox *>("modelSelector");
        auto *input = window.findChild<InputEditor *>("messageInput");
        auto *send = window.findChild<QPushButton *>("SendButton");
        QTRY_COMPARE(combo->count(), 4);
        const auto artifact = qEnvironmentVariable("AIGATEWAY_TEST_ARTIFACT_DIR");
        if (!artifact.isEmpty()) {
            window.resize(1360, 1000);
            QTest::qWait(150);
            QVERIFY(window.grab().save(QDir(artifact).filePath("客户端_欢迎界面.png")));
        }
        input->setFocus();
        QTest::keyClicks(input, "hello");
        QTest::keyClick(input, Qt::Key_Return, Qt::ShiftModifier);
        QVERIFY(input->toPlainText().contains('\n'));
        QVERIFY(server.requests.isEmpty());
        server.chatResponses.append({200, HttpFixture::success("kimi_pro"), 0});
        combo->setCurrentIndex(combo->findData("kimi_pro"));
        QTest::keyClick(input, Qt::Key_Return);
        QTRY_COMPARE(server.requests.size(), 1);
        QCOMPARE(server.requests.last()["model"].toString(), QString("kimi_pro"));
        QTRY_VERIFY(send->text() == "发送");
        QTest::mouseClick(window.findChild<QPushButton *>("NewChat"), Qt::LeftButton);
        server.models = R"({"success":true,"models":[]})";
        QTest::mouseClick(window.findChild<QToolButton *>("refreshModels"), Qt::LeftButton);
        QTRY_COMPARE(combo->count(), 1);
        input->setPlainText("hello");
        QVERIFY(!send->isEnabled());
        server.models = R"({"success":true,"models":[{"id":"kimi_pro","name":"Kimi K2.6","is_mock":false}]})";
        QTest::mouseClick(window.findChild<QToolButton *>("refreshModels"), Qt::LeftButton);
        QTRY_COMPARE(combo->count(), 2);
        QVERIFY(send->isEnabled());
        // At the minimum supported size, folding the sidebar keeps the composer reachable.
        window.resize(980, 700);
        QTest::mouseClick(window.findChild<QToolButton *>("sidebarToggle"), Qt::LeftButton);
        QVERIFY(send->isVisible());
    }
};
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("ClientDeliveryTests");
    applyTheme();
    const auto artifact = qEnvironmentVariable("AIGATEWAY_TEST_ARTIFACT_DIR");
    if (!artifact.isEmpty()) {
        QDir().mkpath(artifact);
        QFile fontInfo(QDir(artifact).filePath("字体检查.txt"));
        if (fontInfo.open(QIODevice::WriteOnly))
            fontInfo.write(("requested=" + qApp->font().family() +
                            "\nactual=" + QFontInfo(qApp->font()).family() + "\nUI available=" +
                            QString::number(QFontDatabase::families().contains("Microsoft YaHei UI")) +
                            "\nYaHei available=" +
                            QString::number(QFontDatabase::families().contains("Microsoft YaHei")))
                               .toUtf8());
    }
    ClientTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "client_tests.moc"
