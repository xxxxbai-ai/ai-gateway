#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QListWidget>
#include <QComboBox>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QMap>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void fetchModelList();
    void onModelsReply(QNetworkReply *reply);
    void onSendButtonClicked();
    void onNetworkReply(QNetworkReply *reply);
    void onRequestTimeout();
    void onNewChatClicked();
    void onToggleSidebar();
    void onHistoryItemClicked(QListWidgetItem *item);

private:
    void setupUI();
    void applyModernStyle();
    void saveCurrentChatState();
    void setSendingState(bool sending); // <--- 补上了这个关键声明！

    QWidget *centralWidget;
    QWidget *sidebarWidget;
    QWidget *mainContentWidget;
    QPushButton *newChatButton;
    QPushButton *toggleSidebarButton;
    QListWidget *historyListWidget;
    QComboBox *modelComboBox;
    QTextEdit *chatDisplayEdit;
    QLineEdit *inputLineEdit;
    QPushButton *sendButton;

    QNetworkAccessManager *networkManager;
    QNetworkAccessManager *modelManager;
    QTimer *timeoutTimer;
    bool isSidebarVisible;

    QMap<QString, QString> chatHistoryMap;
    QString currentChatTitle;
};

#endif // MAINWINDOW_H