#pragma once
#include <QMainWindow>
#include <QJsonObject>
#include <QMap>
#include <functional>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QProcess;
class QPushButton;
class QTextEdit;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QString gatewayUrl = {}, QWidget* parent = nullptr);
    ~MainWindow() override;

signals:
    void modelsLoaded(bool success);
    void chatFinished(bool success, const QJsonObject& result);

private:
    using Handler = std::function<void(bool, const QJsonObject&, const QString&)>;
    void setupUi();
    void startGateway();
    void loadModels();
    void sendQuestion();
    void openSettings();
    void clearMessages();
    void request(const QByteArray& method, const QString& path, const QJsonObject& body, Handler handler);
    void setBusy(bool busy);
    void setConnected(bool connected, const QString& message);
    void showError(const QString& message);
    void appendCard(const QString& title, const QString& text, const QString& details = {});
    QString modelName(const QString& id) const;

    QString base_url_;
    bool connected_ = false;
    bool busy_ = false;
    bool demo_mode_ = false;
    QMap<QString, QString> model_names_;
    QNetworkAccessManager* network_;
    QProcess* gateway_ = nullptr;
    QByteArray startup_output_;
    QComboBox* model_box_;
    QCheckBox* failure_box_;
    QLabel* status_;
    QLineEdit* input_;
    QTextEdit* messages_;
    QPushButton* send_;
    QPushButton* refresh_;
    QPushButton* settings_;
    QPushButton* clear_;
};
