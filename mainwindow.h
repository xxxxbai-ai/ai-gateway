#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QComboBox>
#include <QLabel>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onSendButtonClicked();
    void onNewChatClicked();
    void onToggleSidebar();

private:
    void setupUI();
    void applyModernStyle();

    // 界面主组件
    QWidget *centralWidget;
    QWidget *sidebarWidget;
    QWidget *mainContentWidget;

    // 侧边栏控件
    QPushButton *newChatButton;
    QListWidget *historyListWidget;
    QPushButton *toggleSidebarButton;

    // 主内容区控件
    QComboBox *modelComboBox;      // 模型选择器
    QTextEdit *chatDisplayEdit;    // 聊天消息展示区
    QLineEdit *inputLineEdit;      // 输入框
    QPushButton *sendButton;       // 发送按钮
    bool isSidebarVisible;
};

#endif // MAINWINDOW_H