#include "mainwindow.h"
#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QCoreApplication::setApplicationName("AI 网关模拟演示");
    QCoreApplication::setApplicationVersion("0.2");
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"gateway-url", "连接已运行的网关；默认自动启动随附网关", "url"});
    parser.process(application);
    MainWindow window(parser.value("gateway-url"));
    window.show();
    return application.exec();
}
