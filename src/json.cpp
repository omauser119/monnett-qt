#include "json.h"
#include <QSet>
QByteArray Json::encode(const QJsonValue &v, bool pretty) {
    if (v.isObject())
        return QJsonDocument(v.toObject()).toJson(pretty ? QJsonDocument::Indented : QJsonDocument::Compact);
    if (v.isArray())
        return QJsonDocument(v.toArray()).toJson(pretty ? QJsonDocument::Indented : QJsonDocument::Compact);
    auto b = QJsonDocument(QJsonArray{v}).toJson(QJsonDocument::Compact);
    return b.mid(1, b.size() - 2);
}
QJsonValue Json::parse(const QByteArray &b, QString *err) {
    if (b.trimmed().isEmpty())
        return QJsonValue();
    QJsonParseError e;
    auto d = QJsonDocument::fromJson("[" + b + "]", &e);
    if (e.error != QJsonParseError::NoError) {
        if (err)
            *err = e.errorString();
        return {};
    }
    return d.array().first();
}
QJsonValue Json::unwrap(const QJsonValue &v, QString *err, QString *code) {
    auto o = v.toObject();
    if (!o.contains("success"))
        return v;
    if (o["success"] != true) {
        if (err)
            *err = text(o, {"message", "messageKey", "code"}, "Сервер отклонил запрос");
        if (code)
            *code = o["code"].toString();
        return v;
    }
    if (o.contains("payload"))
        return o["payload"];
    if (o.contains("data"))
        return o["data"];
    return v;
}
QString Json::text(const QJsonObject &o, const QStringList &keys, const QString &f) {
    for (const auto &k : keys) {
        auto v = o.value(k);
        if (v.isString())
            return v.toString();
        if (v.isDouble())
            return QString::number(v.toInteger());
    }
    return f;
}
QString Json::id(const QJsonObject &o) {
    return text(o, {"publicId", "public_id", "id"});
}
QJsonObject Json::object(const QJsonValue &v, const QStringList &keys) {
    auto o = v.toObject();
    for (auto &k : keys)
        if (o[k].isObject())
            return o[k].toObject();
    return o;
}
QJsonArray Json::list(const QJsonValue &v, const QStringList &keys) {
    if (v.isArray())
        return v.toArray();
    auto o = v.toObject();
    auto kk = keys;
    kk.append({"data", "items", "users", "posts", "comments", "replies", "notifications", "referees",
               "results", "files", "likes", "viewers"});
    for (const auto &k : kk)
        if (o[k].isArray())
            return o[k].toArray();
    if (o["data"].isObject())
        return list(o["data"], keys);
    return {};
}
QJsonArray Json::posts(const QJsonValue &v) {
    QJsonArray r;
    auto o = v.toObject();
    if (o["timeline"].isArray()) {
        for (auto group : o["timeline"].toArray())
            for (auto p : group.toObject()["posts"].toArray())
                r.append(p);
        return r;
    }
    for (auto p : list(v)) {
        auto obj = p.toObject();
        if (obj["type"] == "transition")
            continue;
        r.append(obj["post"].isObject() ? obj["post"] : p);
    }
    return r;
}
QJsonValue Json::redact(const QJsonValue &v) {
    if (v.isArray()) {
        QJsonArray a;
        for (auto x : v.toArray())
            a.append(redact(x));
        return a;
    }
    if (!v.isObject())
        return v;
    QJsonObject o;
    auto src = v.toObject();
    for (auto it = src.begin(); it != src.end(); ++it) {
        auto k = it.key().toLower();
        o[it.key()] = (k.contains("token") || k.contains("password") || k == "jwt" || k == "otpdata")
                          ? QJsonValue("[скрыто]")
                          : redact(it.value());
    }
    return o;
}
