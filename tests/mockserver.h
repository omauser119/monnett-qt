#pragma once
#include "json.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <functional>
#include <memory>
struct Captured {
    QByteArray method, target, body;
    QMap<QByteArray, QByteArray> headers;
};
class MockServer : public QTcpServer {
  public:
    QVector<Captured> requests;
    QByteArray rawResponse;
    QByteArray contentType = "application/json";
    std::function<QPair<int, QJsonValue>(const Captured &)> handler;
    MockServer() {
        connect(this, &QTcpServer::newConnection, this, [this] {
            while (hasPendingConnections()) {
                auto socket = nextPendingConnection();
                auto buffer = std::make_shared<QByteArray>();
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket, buffer] {
                    buffer->append(socket->readAll());
                    int end = buffer->indexOf("\r\n\r\n");
                    if (end < 0)
                        return;
                    auto lines = buffer->left(end).split('\n');
                    auto first = lines.takeFirst().trimmed().split(' ');
                    Captured r;
                    r.method = first.value(0);
                    r.target = first.value(1);
                    for (auto line : lines) {
                        auto sep = line.indexOf(':');
                        if (sep > 0)
                            r.headers[line.left(sep).trimmed().toLower()] = line.mid(sep + 1).trimmed();
                    }
                    int size = r.headers.value("content-length").toInt();
                    if (buffer->size() < end + 4 + size)
                        return;
                    r.body = buffer->mid(end + 4, size);
                    requests.append(r);
                    auto response =
                        handler
                            ? handler(r)
                            : qMakePair(200, QJsonValue(QJsonObject{{"success", true},
                                                                    {"payload", QJsonObject{{"ok", true}}}}));
                    auto bytes = rawResponse.isNull() ? Json::encode(response.second) : rawResponse;
                    socket->write("HTTP/1.1 " + QByteArray::number(response.first) +
                                  " Status\r\nContent-Type: " + contentType + "\r\nContent-Length: " +
                                  QByteArray::number(bytes.size()) + "\r\nConnection: close\r\n\r\n" + bytes);
                    socket->disconnectFromHost();
                    buffer->clear();
                });
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            }
        });
        listen(QHostAddress::LocalHost);
    }
    QUrl base() const { return QUrl(QString("http://127.0.0.1:%1").arg(serverPort())); }
};
