#include "accounts.h"
#include "json.h"
#include <QFile>
#include <QSettings>
#include <QUuid>
#pragma push_macro("signals")
#undef signals
#include <libsecret/secret.h>
#pragma pop_macro("signals")
namespace {
const SecretSchema schema = {
    "social.monnett.MonnettQt",
    SECRET_SCHEMA_NONE,
    {{"account", SECRET_SCHEMA_ATTRIBUTE_STRING}, {nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING}},
    0,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr};
QJsonObject publicUser(QJsonObject o) {
    const QStringList privateKeys = {"otpData", "confirmationToken", "resetPasswordToken", "fcmToken",
                                     "referredBy"};
    for (auto &k : privateKeys)
        o.remove(k);
    return o;
}
} // namespace
Accounts::Accounts(QObject *p, bool memory) : QObject(p), m_memoryOnly(memory) {
    if (!memory)
        load();
}
Account *Accounts::find(const QString &key) {
    for (auto &a : m_accounts)
        if (a.key == key)
            return &a;
    return nullptr;
}
void Accounts::load() {
    QSettings s;
    s.beginGroup("accounts");
    m_active = s.value("active").toString();
    auto arr = s.value("metadata").toByteArray();
    for (auto v : QJsonDocument::fromJson(arr).array()) {
        auto o = v.toObject();
        Account a;
        a.key = o["key"].toString();
        a.email = o["email"].toString();
        a.label = o["label"].toString();
        a.deviceId = o["deviceId"].toString();
        a.development = o["development"].toBool();
        a.user = o["user"].toObject();
        GError *error = nullptr;
        auto key = a.key.toUtf8();
        gchar *password =
            secret_password_lookup_sync(&schema, nullptr, &error, "account", key.constData(), nullptr);
        if (password) {
            auto tokens = Json::parse(password).toObject();
            a.jwt = tokens["jwt"].toString();
            a.refresh = tokens["refresh"].toString();
            secret_password_free(password);
        }
        if (error) {
            m_warning = "Не удалось открыть системное хранилище ключей. Войдите заново; токены будут "
                        "храниться только в памяти.";
            g_error_free(error);
        }
        m_accounts.append(a);
    }
    if (!find(m_active) && !m_accounts.isEmpty())
        m_active = m_accounts.first().key;
}
QString Accounts::add(const QJsonObject &s, const QString &email, bool dev) {
    auto u = s["user"].toObject();
    QString uid = Json::id(u);
    QString key;
    for (auto &a : m_accounts)
        if (a.development == dev &&
            ((!uid.isEmpty() && Json::id(a.user) == uid) ||
             (!email.isEmpty() && a.email.compare(email, Qt::CaseInsensitive) == 0))) {
            key = a.key;
            break;
        }
    if (key.isEmpty()) {
        Account a;
        a.key = QUuid::createUuid().toString(QUuid::WithoutBraces);
        a.deviceId = "MONNETTQT_" + QUuid::createUuid().toString(QUuid::Id128);
        a.email = email;
        a.development = dev;
        m_accounts.append(a);
        key = a.key;
    }
    updateSession(key, s);
    m_active = key;
    persist();
    emit changed();
    emit activeChanged();
    return key;
}
void Accounts::updateSession(const QString &key, const QJsonObject &s) {
    if (auto a = find(key)) {
        if (s.contains("jwt"))
            a->jwt = s["jwt"].toString();
        if (s.contains("refresh_token"))
            a->refresh = s["refresh_token"].toString();
        if (s["user"].isObject()) {
            a->user = publicUser(s["user"].toObject());
            a->label = Json::text(a->user, {"username", "name", "email"}, a->email);
        }
        persist();
        emit changed();
    }
}
void Accounts::updateUser(const QString &key, const QJsonObject &u) {
    if (auto a = find(key)) {
        a->user = publicUser(u);
        a->label = Json::text(u, {"username", "name"}, a->email);
        persist();
        emit changed();
    }
}
void Accounts::select(const QString &key) {
    if (key == m_active || !find(key))
        return;
    m_active = key;
    persist();
    emit activeChanged();
}
void Accounts::remove(const QString &key) {
    for (qsizetype i = 0; i < m_accounts.size(); ++i)
        if (m_accounts[i].key == key) {
            m_accounts.removeAt(i);
            break;
        }
    if (!m_memoryOnly) {
        GError *error = nullptr;
        auto k = key.toUtf8();
        secret_password_clear_sync(&schema, nullptr, &error, "account", k.constData(), nullptr);
        if (error) {
            emit storageError(QString::fromUtf8(error->message));
            g_error_free(error);
        }
    }
    if (m_active == key)
        m_active = m_accounts.isEmpty() ? QString() : m_accounts.first().key;
    persist();
    emit changed();
    emit activeChanged();
}
bool Accounts::persist() {
    if (m_memoryOnly)
        return true;
    QJsonArray arr;
    bool ok = true;
    for (const auto &a : m_accounts) {
        arr.append(QJsonObject{{"key", a.key},
                               {"email", a.email},
                               {"label", a.label},
                               {"development", a.development},
                               {"deviceId", a.deviceId},
                               {"user", a.user}});
        if (a.jwt.isEmpty())
            continue;
        auto k = a.key.toUtf8();
        auto data = Json::encode(QJsonObject{{"jwt", a.jwt}, {"refresh", a.refresh}});
        GError *error = nullptr;
        if (!secret_password_store_sync(&schema, SECRET_COLLECTION_DEFAULT,
                                        ("Monnett Qt: " + a.label.toUtf8()).constData(), data.constData(),
                                        nullptr, &error, "account", k.constData(), nullptr)) {
            ok = false;
            m_warning = "Системное хранилище ключей недоступно. Сессия работает в памяти; после выхода "
                        "потребуется вход.";
            emit storageError(m_warning);
        }
        if (error)
            g_error_free(error);
    }
    QSettings s;
    s.beginGroup("accounts");
    s.setValue("active", m_active);
    s.setValue("metadata", QJsonDocument(arr).toJson(QJsonDocument::Compact));
    s.sync();
    QFile::setPermissions(s.fileName(), QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return ok;
}
