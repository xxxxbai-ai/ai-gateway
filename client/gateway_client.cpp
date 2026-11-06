#include "gateway_client.h"
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <memory>

namespace client {
namespace {
constexpr qsizetype responseLimit = 16 * 1024 * 1024;
QJsonObject parsed(const QByteArray &bytes) {
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(bytes, &error);
    return error.error == QJsonParseError::NoError && document.isObject() ? document.object() : QJsonObject();
}
QString errorMessage(const QJsonObject &body, const QString &fallback) {
    QString text =
        body["error"].isString() ? body["error"].toString() : body["error"].toObject()["message"].toString();
    if (text.isEmpty())
        text = body["message"].toString();
    return text.trimmed().isEmpty() ? fallback : text.left(3000);
}
QNetworkRequest requestFor(const QUrl &url) {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("User-Agent", "AI-Gateway-Client/1.0");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    return request;
}
} // namespace
GatewayClient::GatewayClient(QUrl base, QObject *parent, int chatTimeoutMs, int modelsTimeoutMs)
    : QObject(parent), base_(std::move(base)), chatTimeoutMs_(chatTimeoutMs),
      modelsTimeoutMs_(modelsTimeoutMs) {
    chatTimer_.setSingleShot(true);
    modelsTimer_.setSingleShot(true);
    connect(&chatTimer_, &QTimer::timeout, this, [this] {
        abortChat("timeout", "等待回答超时，请稍后重试。也可以缩短问题后再次发送。");
    });
    connect(&modelsTimer_, &QTimer::timeout, this, [this] {
        abortModels();
        emit modelsFailed("连接网关超时，请检查网关是否已启动。");
    });
}
QUrl GatewayClient::normalizeUrl(const QString &value, QString *error) {
    const QUrl url(value.trimmed(), QUrl::StrictMode);
    const auto host = url.host().toLower();
    const bool local = host == "localhost" || host == "127.0.0.1" || host == "::1";
    if (!url.isValid() || host.isEmpty() || !url.userInfo().isEmpty() || url.hasQuery() ||
        url.hasFragment() || (url.scheme() != "https" && !(url.scheme() == "http" && local)) ||
        url.port(1) <= 0) {
        if (error)
            *error = "请输入完整的网关地址。本机可用 http://127.0.0.1:8080，远程地址请使用 HTTPS。";
        return {};
    }
    auto result = url;
    auto path = result.path();
    if (!path.endsWith('/'))
        path += '/';
    result.setPath(path);
    return result;
}
QUrl GatewayClient::endpoint(const QString &path) const {
    return base_.resolved(QUrl(path));
}
void GatewayClient::setBaseUrl(const QUrl &base) {
    abortModels();
    base_ = base;
}
void GatewayClient::abortModels() {
    modelsTimer_.stop();
    const auto reply = modelsReply_;
    modelsReply_.clear();
    if (reply) {
        disconnect(reply, nullptr, this, nullptr);
        reply->abort();
        reply->deleteLater();
    }
}
void GatewayClient::fetchModels() {
    abortModels();
    auto *reply = network_.get(requestFor(endpoint("api/models")));
    modelsReply_ = reply;
    auto bytes = std::make_shared<QByteArray>();
    connect(reply, &QIODevice::readyRead, this, [reply, bytes] {
        bytes->append(reply->readAll());
        if (bytes->size() > 1024 * 1024) {
            reply->setProperty("tooLarge", true);
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, bytes] {
        if (modelsReply_ != reply)
            return;
        modelsReply_.clear();
        modelsTimer_.stop();
        bytes->append(reply->readAll());
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto body = parsed(*bytes);
        const auto networkError = reply->error();
        const bool large = reply->property("tooLarge").toBool();
        reply->deleteLater();
        if (large || status != 200 || networkError != QNetworkReply::NoError || body["success"] != true ||
            !body["models"].isArray()) {
            emit modelsFailed(errorMessage(body, status == 0
                                                     ? "暂时连接不到网关，请先启动网关，或检查设置中的地址。"
                                                     : "网关没有返回有效的模型列表，请刷新重试。"));
            return;
        }
        QJsonArray models;
        QSet<QString> ids;
        for (const auto &value : body["models"].toArray()) {
            const auto model = value.toObject();
            const auto id = model["id"].toString();
            if (id.isEmpty() || id == "auto" || ids.contains(id) || !model["name"].isString() ||
                model["name"].toString().isEmpty() || !model["is_mock"].isBool()) {
                emit modelsFailed("网关模型列表字段不完整，请检查服务配置。");
                return;
            }
            ids.insert(id);
            if (model.contains("enabled") && model["enabled"] == false)
                continue;
            models.append(model);
        }
        emit modelsReady(models);
    });
    modelsTimer_.start(modelsTimeoutMs_);
}
void GatewayClient::sendChat(quint64 token, const QJsonObject &body) {
    if (busy()) {
        emit chatFailed(token, "已有一个问题正在等待回答。", {{"error_code", "busy"}});
        return;
    }
    token_ = token;
    elapsed_.start();
    auto *reply =
        network_.post(requestFor(endpoint("api/chat")), QJsonDocument(body).toJson(QJsonDocument::Compact));
    chatReply_ = reply;
    reply->setReadBufferSize(256 * 1024);
    auto bytes = std::make_shared<QByteArray>();
    connect(reply, &QIODevice::readyRead, this, [reply, bytes] {
        bytes->append(reply->readAll());
        if (bytes->size() > responseLimit) {
            reply->setProperty("tooLarge", true);
            reply->abort();
        }
    });
    const auto manualModel = body["mode"] == "manual" ? body["model"].toString() : QString();
    connect(reply, &QNetworkReply::finished, this, [this, reply, token, bytes, manualModel] {
        if (chatReply_ != reply)
            return;
        chatReply_.clear();
        chatTimer_.stop();
        bytes->append(reply->readAll());
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto networkError = reply->error();
        const bool large = reply->property("tooLarge").toBool();
        reply->deleteLater();
        const auto value = parsed(*bytes);
        QJsonObject details{{"http_status", status},
                            {"request_id", value["request_id"].toString()},
                            {"error_code", value["error_code"].toString()},
                            {"elapsed_ms", elapsed_.elapsed()}};
        if (large || bytes->size() > responseLimit) {
            details["error_code"] = "response_too_large";
            emit chatFailed(token, "回答内容超过可接收范围，请缩短问题后重试。", details);
            return;
        }
        if (status == 0 || networkError == QNetworkReply::ConnectionRefusedError ||
            networkError == QNetworkReply::HostNotFoundError) {
            details["error_code"] = "connection_failed";
            emit chatFailed(token, "连接中断，请检查网关是否仍在运行，再重新发送。", details);
            return;
        }
        if (status < 200 || status >= 300 || value["success"] == false) {
            emit chatFailed(token, errorMessage(value, "模型服务暂时没有完成回答，请稍后重试。"), details);
            return;
        }
        if (networkError != QNetworkReply::NoError || value["success"] != true ||
            !value["answer"].isString() || value["answer"].toString().trimmed().isEmpty() ||
            !value["selected_model"].isString() || value["selected_model"].toString().isEmpty() ||
            !value["is_mock"].isBool() || !value["request_id"].isString() ||
            value["request_id"].toString().isEmpty()) {
            details["error_code"] = "invalid_response";
            emit chatFailed(token, "收到的回答格式不完整，请重试。", details);
            return;
        }
        if (!manualModel.isEmpty() &&
            (value["selected_model"].toString() != manualModel || value["fallback_used"].toBool())) {
            details["error_code"] = "invalid_response";
            emit chatFailed(token, "网关返回的模型与手动选择不一致，请检查网关。", details);
            return;
        }
        emit chatSucceeded(token, value, elapsed_.elapsed());
    });
    chatTimer_.start(chatTimeoutMs_);
}
void GatewayClient::abortChat(const QString &code, const QString &message) {
    if (!chatReply_)
        return;
    auto reply = chatReply_;
    chatReply_.clear();
    chatTimer_.stop();
    disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
    emit chatFailed(token_, message, {{"error_code", code}, {"elapsed_ms", elapsed_.elapsed()}});
}
void GatewayClient::cancelChat() {
    abortChat("cancelled", "已停止等待，可以重新发送或继续提问。");
}
} // namespace client
