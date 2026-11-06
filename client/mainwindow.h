#pragma once
#include "conversation.h"
#include <QHash>
#include <QMainWindow>
#include <QUrl>
#include <memory>

class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QScrollArea;
class QSettings;
class QToolButton;
class QVBoxLayout;
namespace client {
class GatewayClient;
class InputEditor;
class MainWindow : public QMainWindow {
    Q_OBJECT
  public:
    explicit MainWindow(QString dataDirectory, QUrl gateway = {}, QWidget *parent = nullptr);
    ~MainWindow() override;
    const ConversationStore &history() const {
        return store_;
    }
    QString currentConversationId() const {
        return currentId_;
    }

  protected:
    void closeEvent(QCloseEvent *event) override;

  private:
    ConversationStore store_;
    std::unique_ptr<QSettings> settings_;
    GatewayClient *gateway_ = nullptr;
    QFrame *sidebar_ = nullptr;
    QLabel *connection_ = nullptr, *notice_ = nullptr, *inputHint_ = nullptr;
    QLineEdit *search_ = nullptr;
    QListWidget *history_ = nullptr;
    QComboBox *models_ = nullptr;
    QToolButton *refresh_ = nullptr;
    QScrollArea *scroll_ = nullptr;
    QWidget *reader_ = nullptr;
    QVBoxLayout *messages_ = nullptr;
    InputEditor *input_ = nullptr;
    QPushButton *send_ = nullptr;
    QString currentId_, pendingConversation_, pendingMessage_;
    QHash<QString, QString> modelNames_, pendingNames_, drafts_;
    quint64 nextToken_ = 0, pendingToken_ = 0;
    bool modelsReady_ = false;
    void setupUi();
    void rebuildHistory();
    void renderConversation(bool scrollToEnd = true);
    void switchConversation(const QString &id);
    void newConversation();
    void refreshModels();
    void sendCurrent();
    void sendQuestion(const QString &question, const QString &choice);
    void retryQuestion(const QString &messageId);
    void updateSendingState();
    void persist();
    void showNotice(const QString &text);
    void showSettings();
    void exportCurrent();
    void showHistoryMenu(const QPoint &point);
    void onSuccess(quint64 token, const QJsonObject &body, qint64 elapsed);
    void onFailure(quint64 token, const QString &text, const QJsonObject &details);
    void finishRequest(const QString &conversation);
};
} // namespace client
