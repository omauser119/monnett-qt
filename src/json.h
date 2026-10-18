#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QStringList>
namespace Json {
QByteArray encode(const QJsonValue &value, bool pretty = false);
QJsonValue parse(const QByteArray &bytes, QString *error = nullptr);
QJsonValue unwrap(const QJsonValue &value, QString *error = nullptr, QString *code = nullptr);
QString text(const QJsonObject &obj, const QStringList &keys, const QString &fallback = {});
QString id(const QJsonObject &obj); // public UUID preferred for v1; numeric IDs stay explicit at call sites
QJsonArray list(const QJsonValue &value, const QStringList &keys = {});
QJsonArray posts(const QJsonValue &value);
QJsonObject object(const QJsonValue &value, const QStringList &keys = {});
QJsonValue redact(const QJsonValue &value);
}
