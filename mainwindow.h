#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>   // 补上列表头文件
#include <QComboBox>     // 补上下拉框头文件
#include <QLabel>        // 补上标签头文件
#include <QNetworkAccessManager>
#include <QNetworkReply>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onSendButtonClicked();
    void onNewChatClicked();
    void onToggleSidebar();
    void onNetworkReply(QNetworkReply *reply);

private:
    void setupUI();
    void applyModernStyle();

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
};

#endif // MAINWINDOW_H