#pragma once
#include "accounts.h"
#include <QNetworkAccessManager>
#include <QPointer>
#include <QUrlQuery>
#include <functional>
struct ApiResult {
    bool ok = false;
    int status = 0;
    QJsonValue data, raw;
    QString error, code;
    QByteArray responseBody = {};
    QString contentType = {};
};
using ApiCallback = std::function<void(const ApiResult &)>;
class ApiClient : public QObject {
    Q_OBJECT
  public:
    explicit ApiClient(Accounts *accounts, const QString &key, QObject *parent = nullptr);
    explicit ApiClient(const QUrl &base,
                       QObject *parent = nullptr); // unauthenticated login and test transport
    void request(const QByteArray &method, const QString &path, const QUrlQuery &query,
                 const QJsonValue &body, QObject *context, ApiCallback done);
    void get(const QString &path, QObject *context, ApiCallback done, const QUrlQuery &query = {}) {
        request("GET", path, query, {}, context, std::move(done));
    }
    void upload(const QString &path, const QJsonObject &fields, const QStringList &files,
                const QString &fileField, QObject *context, ApiCallback done);
    QString accountKey() const { return m_key; }
    QUrl baseUrl() const;
    static QUrl makeUrl(const QUrl &base, const QString &path, const QUrlQuery &query = {});
    static bool allowedPath(const QString &path);
    static QString segment(const QString &s) { return QString::fromLatin1(QUrl::toPercentEncoding(s)); }
    void setTestBase(const QUrl &base) { m_base = base; }
  signals:
    void sessionExpired();
    void uploadProgress(qint64 sent, qint64 total);

  private:
    void perform(const QByteArray &method, const QString &path, const QUrlQuery &query,
                 const QJsonValue &body, QPointer<QObject> context, ApiCallback done, bool retried);
    void refresh(std::function<void(bool)> done);
    QNetworkRequest networkRequest(const QUrl &url, bool json = true) const;
    static ApiResult result(QNetworkReply *reply);
    Accounts *m_accounts = nullptr;
    QString m_key;
    QUrl m_base;
    QNetworkAccessManager m_network;
    bool m_refreshing = false;
    QVector<std::function<void(bool)>> m_waiting;
};
