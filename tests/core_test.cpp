#include "accounts.h"
#include "api.h"
#include "catalogue.h"
#include "json.h"
#include "mockserver.h"
#include <QFile>
#include <QNetworkReply>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>
class CoreTest : public QObject {
    Q_OBJECT
  private slots:
    void jsonContract() {
        QString e, c;
        QJsonObject bad{{"success", false}, {"message", "quota"}, {"code", "DAILY_LIMIT_REACHED"}};
        QCOMPARE(Json::unwrap(bad, &e, &c), QJsonValue(bad));
        QCOMPARE(e, "quota");
        QCOMPARE(c, "DAILY_LIMIT_REACHED");
        QJsonObject data{{"data", QJsonArray{1, 2}}, {"meta", QJsonObject{{"page", 1}}}};
        QCOMPARE(Json::unwrap(data), QJsonValue(data));
        QCOMPARE(Json::unwrap(QJsonObject{{"success", true}, {"payload", "uuid"}}).toString(), "uuid");
        QCOMPARE(Json::parse(Json::encode("refresh-secret")).toString(), "refresh-secret");
        QCOMPARE(Json::id(QJsonObject{{"id", 15}, {"publicId", "uuid"}}), "uuid");
        auto clean =
            Json::redact(QJsonObject{{"jwt", "hidden"},
                                     {"nested", QJsonObject{{"accessToken", "hidden"}, {"name", "normal"}}}})
                .toObject();
        QCOMPARE(clean["jwt"].toString(), "[скрыто]");
        QCOMPARE(clean["nested"].toObject()["name"].toString(), "normal");
        QJsonObject timeline{
            {"timeline",
             QJsonArray{QJsonObject{{"date_key", "today"}, {"posts", QJsonArray{QJsonObject{{"id", 1}}}}}}}};
        QCOMPARE(Json::posts(timeline).size(), 1);
        QCOMPARE(
            Json::posts(QJsonObject{{"items", QJsonArray{QJsonObject{{"type", "transition"}},
                                                         QJsonObject{{"type", "post"},
                                                                     {"post", QJsonObject{{"id", 2}}}}}}})
                .size(),
            1);
    }
    void urls() {
        auto url = ApiClient::makeUrl(QUrl("https://example.org"),
                                      "/api/v1/app/user/by-username/" + ApiClient::segment("a/b?#"),
                                      QUrlQuery{{"ids", "a"}, {"ids", "b"}, {"keyword", "+ пробел"}});
        QVERIFY(url.toEncoded().contains("a%2Fb%3F%23"));
        QCOMPARE(url.path().section('/', 1, 1), "api");
        QCOMPARE(QUrlQuery(url).allQueryItemValues("ids"), QStringList({"a", "b"}));
        QVERIFY(!ApiClient::allowedPath("/api/v1/admin/posts"));
        QVERIFY(!ApiClient::allowedPath("/api/v1/webhook/brevo"));
        QVERIFY(!ApiClient::allowedPath("/api/%61dmin/posts"));
        QVERIFY(!ApiClient::allowedPath("/api/../admin"));
        QVERIFY(!ApiClient::allowedPath("https://evil.test/api/user"));
        QVERIFY(ApiClient::allowedPath("/api/v1/app/feed"));
    }
    void accountIsolation() {
        Accounts accounts(nullptr, true);
        auto a = accounts.add({{"jwt", "prod"},
                               {"refresh_token", "p-refresh"},
                               {"user", QJsonObject{{"id", 1}, {"publicId", "uuid"}, {"username", "alice"}}}},
                              "alice@example.org", false);
        auto b = accounts.add({{"jwt", "dev"},
                               {"refresh_token", "d-refresh"},
                               {"user", QJsonObject{{"id", 1}, {"publicId", "uuid"}, {"username", "alice"}}}},
                              "alice@example.org", true);
        QVERIFY(a != b);
        QCOMPARE(accounts.all().size(), 2);
        QVERIFY(accounts.find(b)->baseUrl().host().contains("-dev"));
        accounts.select(a);
        accounts.updateSession(b, {{"jwt", "new-dev"}});
        QCOMPARE(accounts.active()->jwt, "prod");
        QCOMPARE(accounts.find(b)->jwt, "new-dev");
        auto again = accounts.add({{"jwt", "new-prod"}, {"user", QJsonObject{{"publicId", "uuid"}}}},
                                  "alice@example.org", false);
        QCOMPARE(again, a);
        QCOMPARE(accounts.all().size(), 2);
        accounts.remove(a);
        QCOMPARE(accounts.activeKey(), b);
    }
    void catalogueCoverage() {
        Catalogue cat;
        auto prod = cat.operations(false);
        auto dev = cat.operations(true);
        QCOMPARE(prod.size(), 167);
        QCOMPARE(dev.size(), prod.size());
        bool referrals = false, legacy = false;
        for (const auto &o : prod) {
            QVERIFY(ApiClient::allowedPath(o.path));
            referrals |= o.path == "/api/v1/app/referrals/me";
            legacy |= o.path == "/api/{storyId}/view";
            QVERIFY(!o.spec.isEmpty());
            for (auto v : o.spec["parameters"].toArray()) {
                auto schema = cat.resolve(v.toObject()["schema"].toObject(), o.schemas);
                QVERIFY(!schema.contains("$ref"));
            }
        }
        QVERIFY(referrals);
        QVERIFY(legacy);
    }
    void httpAndRefresh() {
        MockServer server;
        Accounts accounts(nullptr, true);
        auto key = accounts.add(
            {{"jwt", "expired"}, {"refresh_token", "refresh-string"}, {"user", QJsonObject{{"id", 1}}}},
            "a@example.org", false);
        ApiClient api(&accounts, key);
        api.setTestBase(server.base());
        int refreshes = 0;
        QByteArray refreshBody;
        server.handler = [&](const Captured &r) {
            if (r.target == "/api/refresh") {
                ++refreshes;
                refreshBody = r.body;
                return qMakePair(
                    200, QJsonValue(QJsonObject{
                             {"success", true},
                             {"payload", QJsonObject{{"jwt", "renewed"}, {"refresh_token", "rotated"}}}}));
            }
            if (r.headers.value("authorization") == "Bearer expired")
                return qMakePair(401, QJsonValue(QJsonObject{{"message", "expired"}}));
            return qMakePair(
                200, QJsonValue(QJsonObject{{"success", true}, {"payload", QJsonObject{{"name", "Alice"}}}}));
        };
        int completed = 0;
        bool ok = true;
        for (int i = 0; i < 3; ++i)
            api.get("/api/user", this, [&](const ApiResult &r) {
                ok &= r.ok;
                ++completed;
            });
        QTRY_COMPARE_WITH_TIMEOUT(completed, 3, 5000);
        QVERIFY(ok);
        QCOMPARE(refreshes, 1);
        QCOMPARE(refreshBody, QByteArray("\"refresh-string\""));
        QCOMPARE(accounts.find(key)->jwt, "renewed");
        QCOMPARE(accounts.find(key)->refresh, "rotated");
        for (const auto &r : server.requests) {
            QVERIFY(r.target.startsWith("/api/"));
            QCOMPARE(r.headers.value("accept-language"), QByteArray("en"));
            QVERIFY(!r.headers.value("x-timezone").isEmpty());
        }
    }
    void noWriteRetryOnNetworkFailure() {
        MockServer server;
        server.handler = [](const Captured &) {
            return qMakePair(503, QJsonValue(QJsonObject{{"message", "unavailable"}}));
        };
        ApiClient api(server.base());
        bool finished = false;
        ApiResult result;
        api.request("POST", "/api/follow", {}, QJsonObject{{"subjectId", 7}}, this, [&](const ApiResult &r) {
            result = r;
            finished = true;
        });
        QTRY_VERIFY(finished);
        QVERIFY(!result.ok);
        QCOMPARE(result.status, 503);
        QCOMPARE(server.requests.size(), 1);
    }
    void multipartAndLifetime() {
        MockServer server;
        ApiClient api(server.base());
        QTemporaryDir dir;
        QFile file(dir.filePath("тест.txt"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("fixture-payload");
        file.close();
        bool finished = false;
        api.upload("/api/v1/app/media/upload", {{"postId", "uuid"}, {"mediaIndex", 0}}, {file.fileName()},
                   "file", this, [&](const ApiResult &r) {
                       QVERIFY(r.ok);
                       finished = true;
                   });
        QTRY_VERIFY(finished);
        auto r = server.requests.last();
        QVERIFY(r.headers.value("content-type").contains("multipart/form-data; boundary="));
        QVERIFY(r.body.contains("name=\"postId\""));
        QVERIFY(r.body.contains("name=\"mediaIndex\""));
        QVERIFY(r.body.contains("fixture-payload"));
        bool called = false;
        auto context = new QObject;
        api.get("/api/user", context, [&](const ApiResult &) { called = true; });
        delete context;
        QTest::qWait(100);
        QVERIFY(!called);
    }
};
QTEST_GUILESS_MAIN(CoreTest)
#include "core_test.moc"
