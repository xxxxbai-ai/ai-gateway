#pragma once
#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace client {
struct Message {
    QString id, role, content, status = "complete", replyTo;
    QString modelId, modelName, mode, task, reason, requestId, errorCode;
    bool isMock = false, fallbackUsed = false, uncertain = false;
    qint64 elapsedMs = 0;
    int httpStatus = 0;
    QDateTime created = QDateTime::currentDateTimeUtc();
    QJsonObject toJson() const;
    static Message fromJson(const QJsonObject &value);
};
struct Conversation {
    QString id, title = "新对话", selectedModel = "auto";
    bool customTitle = false;
    QDateTime created = QDateTime::currentDateTimeUtc(), updated = created;
    QVector<Message> messages;
    QJsonObject toJson() const;
    static Conversation fromJson(const QJsonObject &value);
};
class ConversationStore {
  public:
    explicit ConversationStore(QString directory);
    QVector<Conversation> conversations;
    bool load(QString *warning = nullptr);
    bool save(QString *error = nullptr) const;
    QString filePath() const;
    QString directory() const {
        return directory_;
    }
    Conversation *find(const QString &id);
    Message *findMessage(const QString &conversation, const QString &message);
    QString create();
    bool remove(const QString &id);

  private:
    QString directory_;
    bool writable_ = true;
};
QString newId();
QString friendlyName(QString name);
QString taskName(const QString &task);
QString exportMarkdown(const Conversation &conversation);
} // namespace client
