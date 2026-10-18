#include "catalogue.h"
#include "api.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
Catalogue::Catalogue() {
    QFile f(":/catalogue.json");
    if (!f.open(QIODevice::ReadOnly))
        qFatal("Cannot load API catalogue");
    m_root = QJsonDocument::fromJson(f.readAll()).object();
}
QVector<Operation> Catalogue::operations(bool dev) const {
    QVector<Operation> v;
    auto env = dev ? "dev" : "prod";
    for (auto value : m_root["operations"].toArray()) {
        auto o = value.toObject();
        if (o["environment"] != env || !ApiClient::allowedPath(o["path"].toString()))
            continue;
        Operation op;
        op.method = o["method"].toString();
        op.path = o["path"].toString();
        op.summary = o["summary"].toString();
        op.source = o["source"].toString();
        op.environment = env;
        op.spec = o;
        op.schemas = op.source == "reverse" ? m_root["reverseSchemas"].toObject()
                                            : m_root["schemas"].toObject()[env].toObject();
        v.append(op);
    }
    return v;
}
QJsonObject Catalogue::resolve(const QJsonObject &s, const QJsonObject &schemas) const {
    auto o = s;
    QSet<QString> seen;
    while (o.contains("$ref")) {
        auto name = o["$ref"].toString().section('/', -1);
        if (seen.contains(name))
            break;
        seen.insert(name);
        auto ref = schemas[name].toObject();
        o.remove("$ref");
        for (auto it = ref.begin(); it != ref.end(); ++it)
            if (!o.contains(it.key()))
                o[it.key()] = it.value();
    }
    return o;
}
QJsonValue Catalogue::example(const QJsonObject &schema, const QJsonObject &schemas, int d) const {
    if (d > 5)
        return {};
    auto s = resolve(schema, schemas);
    if (s.contains("example"))
        return s["example"];
    if (s.contains("default"))
        return s["default"];
    if (!s["examples"].toArray().isEmpty())
        return s["examples"].toArray().first();
    if (!s["enum"].toArray().isEmpty())
        return s["enum"].toArray().first();
    auto t = s["type"].toString();
    if (t == "object" || s.contains("properties")) {
        QJsonObject o;
        auto props = s["properties"].toObject();
        for (auto it = props.begin(); it != props.end(); ++it) {
            if (it.value().toObject()["readOnly"].toBool() || !s["required"].toArray().contains(it.key()))
                continue;
            o[it.key()] = example(it.value().toObject(), schemas, d + 1);
        }
        return o;
    }
    if (t == "array")
        return QJsonArray{};
    if (t == "integer" || t == "number")
        return s.value("minimum").toDouble(0);
    if (t == "boolean")
        return false;
    return "";
}
