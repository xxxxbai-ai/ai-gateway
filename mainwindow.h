#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QComboBox>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onSendButtonClicked();
    void onNewChatClicked();
    void onToggleSidebar();
    void fetchModelList(); // 新增：请求获取模型列表
    void onNetworkReply(QNetworkReply *reply);
    void onModelsReply(QNetworkReply *reply); // 新增：处理模型列表响应
    void onRequestTimeout(); // 新增：处理请求超时

private:
    void setupUI();
    void applyModernStyle();
    void setSendingState(bool sending); // 新增：控制发送中状态

    QWidget *centralWidget;
    QWidget *sidebarWidget;
    QWidget *mainContentWidget;

    QPushButton *newChatButton;
    QListWidget *historyListWidget;
    QPushButton *toggleSidebarButton;

    QComboBox *modelComboBox;
    QTextEdit *chatDisplayEdit;
    QLineEdit *inputLineEdit;
    QPushButton *sendButton;
    bool isSidebarVisible;

    QNetworkAccessManager *networkManager;
    QNetworkAccessManager *modelManager; // 专门用于获取模型列表的网络管理器
    QTimer *timeoutTimer; // 新增：130秒超时定时器
};

#endif // MAINWINDOW_H