#include "conversation.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUuid>
#include <stdexcept>

namespace client {
QString newId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
QString friendlyName(QString name) {
    // Region remains in the gateway configuration; simplify the presentation label only.
    name.remove(QRegularExpression(QStringLiteral("\\s*[（(]北京[）)]\\s*$")));
    return name;
}
QString taskName(const QString &task) {
    const QMap<QString, QString> names{{"programming", "编程"},
                                       {"math", "数学"},
                                       {"translation", "翻译"},
                                       {"writing", "写作"},
                                       {"general", "通用"}};
    return names.value(task, task);
}
QJsonObject Message::toJson() const {
    return {{"id", id},
            {"role", role},
            {"content", content},
            {"status", status},
            {"reply_to", replyTo},
            {"model_id", modelId},
            {"model_name", modelName},
            {"mode", mode},
            {"task", task},
            {"reason", reason},
            {"request_id", requestId},
            {"error_code", errorCode},
            {"is_mock", isMock},
            {"fallback_used", fallbackUsed},
            {"uncertain", uncertain},
            {"elapsed_ms", elapsedMs},
            {"http_status", httpStatus},
            {"created", created.toString(Qt::ISODateWithMs)}};
}
Message Message::fromJson(const QJsonObject &v) {
    if (!v["id"].isString() || v["id"].toString().isEmpty() || !v["content"].isString() ||
        (v["role"] != "user" && v["role"] != "assistant"))
        throw std::runtime_error("invalid message");
    Message m;
    m.id = v["id"].toString();
    m.role = v["role"].toString();
    m.content = v["content"].toString();
    m.status = v["status"].toString("complete");
    m.replyTo = v["reply_to"].toString();
    const QSet<QString> states{"complete", "pending", "error", "cancelled"};
    if (!states.contains(m.status))
        throw std::runtime_error("invalid message state");
    if (m.status == "pending") {
        m.status = "cancelled";
        m.content = "上次等待已结束，可以重新发送这个问题。";
    }
    m.modelId = v["model_id"].toString();
    m.modelName = v["model_name"].toString();
    m.mode = v["mode"].toString();
    m.task = v["task"].toString();
    m.reason = v["reason"].toString();
    m.requestId = v["request_id"].toString();
    m.errorCode = v["error_code"].toString();
    m.isMock = v["is_mock"].toBool();
    m.fallbackUsed = v["fallback_used"].toBool();
    m.uncertain = v["uncertain"].toBool();
    m.elapsedMs = v["elapsed_ms"].toInteger();
    m.httpStatus = v["http_status"].toInt();
    m.created = QDateTime::fromString(v["created"].toString(), Qt::ISODateWithMs);
    if (!m.created.isValid())
        m.created = QDateTime::currentDateTimeUtc();
    return m;
}
QJsonObject Conversation::toJson() const {
    QJsonArray items;
    for (const auto &m : messages)
        items.append(m.toJson());
    return {{"id", id},
            {"title", title},
            {"selected_model", selectedModel},
            {"custom_title", customTitle},
            {"created", created.toString(Qt::ISODateWithMs)},
            {"updated", updated.toString(Qt::ISODateWithMs)},
            {"messages", items}};
}
Conversation Conversation::fromJson(const QJsonObject &v) {
    if (!v["id"].isString() || v["id"].toString().isEmpty() || !v["title"].isString() ||
        !v["messages"].isArray())
        throw std::runtime_error("invalid conversation");
    Conversation c;
    c.id = v["id"].toString();
    c.title = v["title"].toString();
    c.selectedModel = v["selected_model"].toString("auto");
    c.customTitle = v["custom_title"].toBool();
    c.created = QDateTime::fromString(v["created"].toString(), Qt::ISODateWithMs);
    c.updated = QDateTime::fromString(v["updated"].toString(), Qt::ISODateWithMs);
    if (!c.created.isValid())
        c.created = QDateTime::currentDateTimeUtc();
    if (!c.updated.isValid())
        c.updated = c.created;
    QSet<QString> ids;
    for (const auto &item : v["messages"].toArray()) {
        auto m = Message::fromJson(item.toObject());
        if (ids.contains(m.id))
            throw std::runtime_error("duplicate message");
        ids.insert(m.id);
        c.messages.append(std::move(m));
    }
    return c;
}
ConversationStore::ConversationStore(QString directory) : directory_(std::move(directory)) {}
QString ConversationStore::filePath() const {
    return QDir(directory_).filePath("history.json");
}
bool ConversationStore::load(QString *warning) {
    conversations.clear();
    QFile input(filePath());
    if (!input.exists())
        return true;
    bool valid = input.open(QIODevice::ReadOnly);
    if (!valid) {
        writable_ = false;
        if (warning)
            *warning = "历史记录暂时无法读取，本次不会覆盖原文件。";
        return false;
    }
    try {
        if (input.size() > 64 * 1024 * 1024)
            throw std::runtime_error("history too large");
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(input.readAll(), &error);
        const auto root = document.object();
        if (error.error != QJsonParseError::NoError || !document.isObject() || root["version"] != 1 ||
            !root["conversations"].isArray())
            throw std::runtime_error("invalid history");
        QSet<QString> ids;
        for (const auto &item : root["conversations"].toArray()) {
            auto c = Conversation::fromJson(item.toObject());
            if (ids.contains(c.id))
                throw std::runtime_error("duplicate conversation");
            ids.insert(c.id);
            conversations.append(std::move(c));
        }
        return true;
    } catch (...) {
        input.close();
        conversations.clear();
        const auto backup =
            filePath() + ".unreadable-" + QDateTime::currentDateTimeUtc().toString("yyyyMMdd-hhmmss-zzz");
        writable_ = QFile::copy(filePath(), backup);
        if (warning)
            *warning = writable_ ? "原历史文件格式异常，已保留备份并开启空白会话。"
                                 : "历史文件无法恢复，本次不会覆盖原文件。";
        return false;
    }
}
bool ConversationStore::save(QString *error) const {
    if (!writable_ || !QDir().mkpath(directory_)) {
        if (error)
            *error = "无法保存历史记录，本次内容仍可复制或导出。";
        return false;
    }
    QJsonArray items;
    for (const auto &c : conversations)
        items.append(c.toJson());
    QSaveFile output(filePath());
    const auto bytes =
        QJsonDocument(QJsonObject{{"version", 1}, {"conversations", items}}).toJson(QJsonDocument::Compact);
    if (!output.open(QIODevice::WriteOnly) || output.write(bytes) != bytes.size() || !output.commit()) {
        if (error)
            *error = "历史记录保存失败，可先复制或导出当前对话。";
        return false;
    }
    return true;
}
Conversation *ConversationStore::find(const QString &id) {
    for (auto &c : conversations)
        if (c.id == id)
            return &c;
    return nullptr;
}
Message *ConversationStore::findMessage(const QString &conversation, const QString &message) {
    if (auto *c = find(conversation))
        for (auto &m : c->messages)
            if (m.id == message)
                return &m;
    return nullptr;
}
QString ConversationStore::create() {
    Conversation c;
    c.id = newId();
    const auto id = c.id;
    conversations.prepend(std::move(c));
    return id;
}
bool ConversationStore::remove(const QString &id) {
    for (qsizetype i = 0; i < conversations.size(); ++i)
        if (conversations[i].id == id) {
            conversations.removeAt(i);
            return true;
        }
    return false;
}
QString exportMarkdown(const Conversation &c) {
    QString result = "# " + c.title + "\n\n";
    for (const auto &m : c.messages) {
        result += (m.role == "user" ? "## 你\n\n"
                                    : "## AI Gateway" +
                                          (m.modelName.isEmpty() ? QString() : " · " + m.modelName) + "\n\n");
        result += (m.status == "pending" ? QString("（正在等待回答）") : m.content) + "\n\n";
    }
    return result;
}
} // namespace client
