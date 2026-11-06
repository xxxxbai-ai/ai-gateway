#pragma once
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QTimer>
#include <QUrl>

class QNetworkReply;
namespace client {
class GatewayClient : public QObject {
    Q_OBJECT
  public:
    explicit GatewayClient(QUrl base, QObject *parent = nullptr, int chatTimeoutMs = 130000,
                           int modelsTimeoutMs = 8000);
    static QUrl normalizeUrl(const QString &value, QString *error = nullptr);
    QUrl baseUrl() const {
        return base_;
    }
    void setBaseUrl(const QUrl &base);
    void fetchModels();
    void sendChat(quint64 token, const QJsonObject &request);
    void cancelChat();
    bool busy() const {
        return !chatReply_.isNull();
    }
  signals:
    void modelsReady(const QJsonArray &models);
    void modelsFailed(const QString &message);
    void chatSucceeded(quint64 token, const QJsonObject &response, qint64 elapsedMs);
    void chatFailed(quint64 token, const QString &message, const QJsonObject &details);

  private:
    QNetworkAccessManager network_;
    QUrl base_;
    QPointer<QNetworkReply> modelsReply_, chatReply_;
    QTimer modelsTimer_, chatTimer_;
    QElapsedTimer elapsed_;
    quint64 token_ = 0;
    int chatTimeoutMs_, modelsTimeoutMs_;
    QUrl endpoint(const QString &path) const;
    void abortChat(const QString &code, const QString &message);
    void abortModels();
};
} // namespace client
