#include "gateway_client.h"
#include "mainwindow.h"
#include "widgets.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QLibraryInfo>
#include <QLockFile>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTranslator>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName("AI Gateway Team");
    QApplication::setApplicationName("AIGatewayClient");
    QApplication::setApplicationVersion("1.0.0");
    client::applyTheme();
    app.setWindowIcon(client::appIcon());
    QTranslator translator;
    if (translator.load("qtbase_zh_CN",
                        QDir(QCoreApplication::applicationDirPath()).filePath("translations")) ||
        translator.load("qtbase_zh_CN", QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        app.installTranslator(&translator);
    QCommandLineParser parser;
    parser.setApplicationDescription("AI Gateway 桌面客户端");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"data-dir", "本机历史和设置保存目录", "directory"});
    parser.addOption({"gateway", "网关地址", "url"});
    parser.process(app);
    auto directory = parser.value("data-dir");
    if (directory.isEmpty())
        directory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(directory);
    QLockFile lock(QDir(directory).filePath("client.lock"));
    if (!lock.tryLock(0)) {
        QMessageBox::information(nullptr, "AI Gateway",
                                 lock.error() == QLockFile::LockFailedError
                                     ? "客户端已在运行，请返回已打开的窗口。"
                                     : "无法打开历史保存目录，请检查文件夹权限。");
        return 1;
    }
    QUrl gateway;
    if (parser.isSet("gateway")) {
        QString error;
        gateway = client::GatewayClient::normalizeUrl(parser.value("gateway"), &error);
        if (gateway.isEmpty()) {
            QMessageBox::warning(nullptr, "地址不正确", error);
            return 2;
        }
    }
    client::MainWindow window(directory, gateway);
    window.show();
    return app.exec();
}
