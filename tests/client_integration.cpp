#include "mainwindow.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QLineEdit>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTest>
#include <QTextEdit>
#include <QTimer>
#include <memory>

class ClientIntegration : public QObject {
    Q_OBJECT
private:
    QProcess gateway_;
    std::unique_ptr<QTemporaryDir> temporary_;
    QString url_;
    QString bin_ = QCoreApplication::applicationDirPath();

    bool startGateway(int port = 0) {
        gateway_.setProcessChannelMode(QProcess::MergedChannels);
        gateway_.start(bin_ + "/gateway.exe", {"--demo", "--port", QString::number(port),
            "--config", temporary_->path() + "/models.json", "--log", temporary_->path() + "/requests.jsonl"});
        if (!gateway_.waitForStarted(5000)) return false;
        QByteArray output;
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 5000) {
            gateway_.waitForReadyRead(100);
            output += gateway_.readAll();
            const auto match = QRegularExpression("Gateway ready: (http://127\\.0\\.0\\.1:[0-9]+)")
                .match(QString::fromUtf8(output));
            if (match.hasMatch()) { url_ = match.captured(1); return true; }
            if (gateway_.state() == QProcess::NotRunning) break;
        }
        qWarning().noquote() << output;
        return false;
    }

    static bool send(MainWindow& window, const QString& question, QJsonObject& result) {
        auto* input = window.findChild<QLineEdit*>("messageInput");
        auto* button = window.findChild<QPushButton*>("sendButton");
        if (!input || !button || !button->isEnabled()) return false;
        QSignalSpy completed(&window, &MainWindow::chatFinished);
        input->setText(question);
        QTest::mouseClick(button, Qt::LeftButton);
        if (completed.isEmpty() && !completed.wait(5000)) return false;
        const auto arguments = completed.takeFirst();
        result = qvariant_cast<QJsonObject>(arguments[1]);
        return arguments[0].toBool();
    }

private slots:
    void initTestCase() {
        // The offscreen plugin does not discover Windows system fonts itself.
        if (qEnvironmentVariable("QT_QPA_PLATFORM") == "offscreen") {
            const auto fonts = qEnvironmentVariable("WINDIR") + "/Fonts/";
            QFontDatabase::addApplicationFont(fonts + "msyh.ttc");
            QFontDatabase::addApplicationFont(fonts + "msyhbd.ttc");
        }
        QVERIFY(QDir().mkpath(bin_ + "/test-work"));
        temporary_ = std::make_unique<QTemporaryDir>(bin_ + "/test-work/gui-XXXXXX");
        QVERIFY(temporary_->isValid());
        QVERIFY(QFile::copy(bin_ + "/config/models.default.json", temporary_->path() + "/models.json"));
        QVERIFY(startGateway());
    }

    void completeWorkflow() {
        MainWindow window(url_);
        QSignalSpy loaded(&window, &MainWindow::modelsLoaded);
        window.show();
        QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 5000);
        QVERIFY(loaded.takeFirst()[0].toBool());
        auto* models = window.findChild<QComboBox*>("modelComboBox");
        auto* messages = window.findChild<QTextEdit*>("chatDisplay");
        auto* failure = window.findChild<QCheckBox*>("failureCheckBox");
        auto* sendButton = window.findChild<QPushButton*>("sendButton");
        QCOMPARE(models->count(), 4);
        QVERIFY(failure->isEnabled());

        QJsonObject result;
        QVERIFY(send(window, "解释 C++ 指针", result));
        QCOMPARE(result["selected_model"].toString(), "mock_code");
        QCOMPARE(result["task"].toString(), "programming");
        QVERIFY(messages->toPlainText().contains("模拟回答"));
        QVERIFY(messages->toPlainText().contains("解释 C++ 指针"));
        QVERIFY(messages->toPlainText().contains("选择理由"));
        QVERIFY(send(window, "求解方程 x+2=5", result));
        QCOMPARE(result["selected_model"].toString(), "mock_math");
        QCOMPARE(result["task"].toString(), "math");
        QVERIFY(send(window, "把你好翻译成英文", result));
        QCOMPARE(result["selected_model"].toString(), "mock");
        QCOMPARE(result["task"].toString(), "translation");
        QVERIFY(send(window, "写一封请假信", result));
        QCOMPARE(result["task"].toString(), "writing");
        QVERIFY(send(window, "你好呀", result));
        QCOMPARE(result["task"].toString(), "general");

        models->setCurrentIndex(models->findData("mock_math"));
        QVERIFY(send(window, "解释 C++ 指针", result));
        QCOMPARE(result["selected_model"].toString(), "mock_math");
        QVERIFY(!result["fallback_used"].toBool());

        models->setCurrentIndex(0);
        failure->setChecked(true);
        QVERIFY(send(window, "解释 C++ 指针", result));
        QVERIFY(result["fallback_used"].toBool());
        QCOMPARE(result["selected_model"].toString(), "mock");
        QVERIFY(messages->toPlainText().contains("已切换备用模型"));

        models->setCurrentIndex(models->findData("mock_code"));
        QVERIFY(!send(window, "解释 C++ 指针", result));
        QVERIFY(!result["success"].toBool());
        QVERIFY(sendButton->isEnabled());
        failure->setChecked(false);
        models->setCurrentIndex(0);
        QTest::mouseClick(sendButton, Qt::LeftButton);
        QVERIFY(messages->toPlainText().contains("请输入问题后再发送"));

        // The real settings dialog saves through HTTP and affects the next request.
        QTimer poll;
        bool edited = false;
        connect(&poll, &QTimer::timeout, &window, [&] {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (!dialog || dialog->objectName() != "settingsDialog") return;
            auto* score = dialog->findChild<QSpinBox*>("score_mock_math_programming");
            auto* save = dialog->findChild<QPushButton*>("saveSettingsButton");
            if (!score || !save) { dialog->reject(); return; }
            score->setValue(100);
            edited = true;
            poll.stop();
            QTest::mouseClick(save, Qt::LeftButton);
        });
        loaded.clear();
        poll.start(20);
        QTest::mouseClick(window.findChild<QPushButton*>("settingsButton"), Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 5000);
        poll.stop();
        QVERIFY(edited);
        QVERIFY(loaded.takeFirst()[0].toBool());
        QVERIFY(send(window, "解释 C++ 指针", result));
        QCOMPARE(result["selected_model"].toString(), "mock_math");
        QFile saved(temporary_->path() + "/models.json");
        QVERIFY(saved.open(QIODevice::ReadOnly));
        QVERIFY(saved.readAll().contains("100"));

        QTest::mouseClick(window.findChild<QPushButton*>("clearButton"), Qt::LeftButton);
        QVERIFY(!messages->toPlainText().contains("请求编号"));
        QVERIFY(send(window, "解释 C++ 指针", result));
        failure->setChecked(true);
        QVERIFY(send(window, "求解方程 x+2=5", result));
        QVERIFY(result["fallback_used"].toBool());
        QVERIFY(QDir().mkpath(bin_ + "/../artifacts"));
        QTest::qWait(80);
        QVERIFY(window.grab().save(bin_ + "/../artifacts/client-demo.png"));
    }

    void disconnectAndRecover() {
        MainWindow window(url_);
        QSignalSpy loaded(&window, &MainWindow::modelsLoaded);
        window.show();
        QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 5000);
        QVERIFY(loaded.takeFirst()[0].toBool());
        const int port = url_.section(':', -1).toInt();
        gateway_.kill();
        QVERIFY(gateway_.waitForFinished(3000));
        QJsonObject result;
        QVERIFY(!send(window, "你好", result));
        QVERIFY(window.findChild<QTextEdit*>("chatDisplay")->toPlainText().contains("无法连接到网关"));
        QVERIFY(!window.findChild<QPushButton*>("sendButton")->isEnabled());
        QVERIFY(startGateway(port));
        QTest::mouseClick(window.findChild<QPushButton*>("refreshButton"), Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 5000);
        QVERIFY(loaded.takeFirst()[0].toBool());
        QVERIFY(send(window, "你好", result));
    }

    void standaloneStartsOwnGateway() {
        MainWindow window;
        QSignalSpy loaded(&window, &MainWindow::modelsLoaded);
        window.show();
        QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty(), 12000);
        QVERIFY(loaded.takeFirst()[0].toBool());
        QJsonObject result;
        QVERIFY(send(window, "解释 C++ 指针", result));
        QVERIFY(result["success"].toBool());
        QVERIFY(result["is_mock"].toBool());
    }

    void cleanupTestCase() {
        if (gateway_.state() != QProcess::NotRunning) {
            gateway_.kill();
            gateway_.waitForFinished(3000);
        }
    }
};

QTEST_MAIN(ClientIntegration)
#include "client_integration.moc"
