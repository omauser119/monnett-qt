#include "api.h"
#include "json.h"
#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QMimeDatabase>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QTimeZone>
#include <utility>
namespace {
bool liveContext(const QPointer<QObject> &context) {
    if (!context)
        return false;
    for (auto object = context.data(); object; object = object->parent())
        if (object->property("monnettAbandoned").toBool())
            return false;
    return true;
}
} // namespace

ApiClient::ApiClient(Accounts *a, const QString &key, QObject *p)
    : QObject(p), m_accounts(a), m_key(key), m_network(this) {}
ApiClient::ApiClient(const QUrl &base, QObject *p) : QObject(p), m_base(base), m_network(this) {}
QUrl ApiClient::baseUrl() const {
    if (!m_base.isEmpty())
        return m_base;
    if (m_accounts)
        if (auto a = m_accounts->find(m_key))
            return a->baseUrl();
    return {};
}
bool ApiClient::allowedPath(const QString &p) {
    const auto decoded = QUrl::fromPercentEncoding(p.toUtf8()).toLower();
    const QRegularExpression excluded("^/api/(?:admin(?:/|$)|v1/(?:admin|webhook)(?:/|$)|q/)");
    return decoded.startsWith("/api/") && !excluded.match(decoded).hasMatch() && !decoded.contains("..") &&
           !p.contains('?') && !p.contains('#') && !decoded.contains('\\');
}

QUrl ApiClient::makeUrl(const QUrl &base, const QString &path, const QUrlQuery &q) {
    if (!allowedPath(path))
        return {};
    auto url = base;
    url.setPath(path, QUrl::TolerantMode);
    url.setQuery(q);
    return url;
}
QNetworkRequest ApiClient::networkRequest(const QUrl &url, bool json) const {
    QNetworkRequest r(url);
    r.setTransferTimeout(45000);
    r.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::SameOriginRedirectPolicy);
    r.setRawHeader("Accept", "application/json");
    r.setRawHeader("Accept-Language", "en");
    r.setRawHeader("X-Timezone", QTimeZone::systemTimeZoneId());
    r.setRawHeader("User-Agent", "MonnettQt/0.1 (Linux)");
    if (json)
        r.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    static const QSet<QString> publicPaths = {"/api/auth/login", "/api/auth/register", "/api/refresh",
                                              "/api/send-otp",   "/api/verify-otp",    "/api/reset-password"};
    if (m_accounts && !publicPaths.contains(url.path()))
        if (auto a = m_accounts->find(m_key); a && !a->jwt.isEmpty())
            r.setRawHeader("Authorization", "Bearer " + a->jwt.toUtf8());
    return r;
}
ApiResult ApiClient::result(QNetworkReply *r) {
    ApiResult out;
    out.status = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    auto bytes = r->readAll();
    out.responseBody = bytes;
    out.contentType = r->header(QNetworkRequest::ContentTypeHeader).toString();
    QString parseError;
    out.raw = Json::parse(bytes, &parseError);
    out.data = Json::unwrap(out.raw, &out.error, &out.code);
    out.ok =
        r->error() == QNetworkReply::NoError && out.status >= 200 && out.status < 300 && out.error.isEmpty();
    if (!out.ok && out.error.isEmpty())
        out.error = Json::text(out.raw.toObject(), {"message", "error", "errcode"}, r->errorString());
    if (!parseError.isEmpty()) {
        if (out.contentType.contains("json", Qt::CaseInsensitive)) {
            if (out.ok) {
                out.ok = false;
                out.error = "Сервер вернул некорректный JSON: " + parseError;
            }
        } else {
            out.raw =
                out.contentType.startsWith("text/")
                    ? QJsonValue(QString::fromUtf8(bytes))
                    : QJsonValue(QJsonObject{{"content_type", out.contentType}, {"bytes", bytes.size()}});
            out.data = out.raw;
        }
    }
    return out;
}
void ApiClient::request(const QByteArray &m, const QString &p, const QUrlQuery &q, const QJsonValue &b,
                        QObject *c, ApiCallback done) {
    perform(m, p, q, b, c, std::move(done), false);
}
void ApiClient::perform(const QByteArray &m, const QString &p, const QUrlQuery &q, const QJsonValue &b,
                        QPointer<QObject> c, ApiCallback done, bool retried) {
    if (!liveContext(c))
        return;
    auto url = makeUrl(baseUrl(), p, q);
    if (!url.isValid() || url.host().isEmpty()) {
        done({false, 0, {}, {}, "Недопустимый путь API или удалённый аккаунт", {}});
        return;
    }
    auto req = networkRequest(url);
    auto reply =
        m_network.sendCustomRequest(req, m, b.isUndefined() || b.isNull() ? QByteArray() : Json::encode(b));
    connect(reply, &QNetworkReply::finished, this, [=, this] {
        auto r = result(reply);
        reply->deleteLater();
        if (!liveContext(c))
            return;
        if (r.status == 401 && !retried && m_accounts && p != "/api/refresh") {
            if (auto account = m_accounts->find(m_key);
                account && !account->jwt.isEmpty() &&
                req.rawHeader("Authorization") != "Bearer " + account->jwt.toUtf8()) {
                perform(m, p, q, b, c, done, true);
                return;
            }
            refresh([=, this](bool ok) {
                if (!liveContext(c))
                    return;
                if (ok)
                    perform(m, p, q, b, c, done, true);
                else {
                    emit sessionExpired();
                    done(r);
                }
            });
            return;
        }
        done(r);
    });
}
void ApiClient::refresh(std::function<void(bool)> done) {
    m_waiting.append(std::move(done));
    if (m_refreshing)
        return;
    auto a = m_accounts ? m_accounts->find(m_key) : nullptr;
    if (!a || a->refresh.isEmpty()) {
        auto wait = std::exchange(m_waiting, {});
        for (auto &f : wait)
            f(false);
        return;
    }
    m_refreshing = true;
    const auto token = a->refresh;
    perform(
        "POST", "/api/refresh", {}, token, this,
        [this](const ApiResult &r) {
            bool ok = r.ok && !r.data.toObject()["jwt"].toString().isEmpty();
            if (ok)
                m_accounts->updateSession(m_key, r.data.toObject());
            m_refreshing = false;
            auto waiting = std::exchange(m_waiting, {});
            for (auto &f : waiting)
                f(ok);
        },
        true);
}
void ApiClient::upload(const QString &p, const QJsonObject &fields, const QStringList &files,
                       const QString &fileField, QObject *c, ApiCallback done) {
    if (!liveContext(c))
        return;
    auto url = makeUrl(baseUrl(), p);
    if (url.isEmpty()) {
        done({false, 0, {}, {}, "Недопустимый путь загрузки", {}});
        return;
    }
    auto multi = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    for (auto it = fields.begin(); it != fields.end(); ++it) {
        QHttpPart part;
        part.setHeader(QNetworkRequest::ContentDispositionHeader, "form-data; name=\"" + it.key() + "\"");
        part.setBody(it.value().isString() ? it.value().toString().toUtf8() : Json::encode(it.value()));
        multi->append(part);
    }
    for (const auto &name : files) {
        auto f = new QFile(name, multi);
        if (!f->open(QIODevice::ReadOnly)) {
            delete multi;
            done({false, 0, {}, {}, "Не удалось открыть файл: " + QFileInfo(name).fileName(), {}});
            return;
        }
        QString safe = QFileInfo(name).fileName();
        safe.replace('"', '_');
        safe.replace('\r', '_');
        safe.replace('\n', '_');
        QHttpPart part;
        part.setHeader(QNetworkRequest::ContentDispositionHeader,
                       "form-data; name=\"" + fileField + "\"; filename=\"" + safe + "\"");
        part.setHeader(QNetworkRequest::ContentTypeHeader, QMimeDatabase().mimeTypeForFile(name).name());
        part.setBodyDevice(f);
        multi->append(part);
    }
    auto req = networkRequest(url, false);
    req.setTransferTimeout(300000);
    auto reply = m_network.post(req, multi);
    multi->setParent(reply);
    connect(reply, &QNetworkReply::uploadProgress, this, &ApiClient::uploadProgress);
    QPointer<QObject> context = c;
    connect(reply, &QNetworkReply::finished, this, [reply, context, done] {
        auto r = result(reply);
        reply->deleteLater();
        if (liveContext(context))
            done(r);
    });
}
