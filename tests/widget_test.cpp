#include "authdialog.h"
#include "mockserver.h"
#include "operationpanel.h"
#include "panels.h"
#include "widgets.h"
#include "window.h"
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QtTest>
#include <algorithm>
class WidgetTest : public QObject {
    Q_OBJECT
  private slots:
    void accountGesture() {
        AddAccountButton b;
        b.resize(240, 45);
        b.show();
        QSignalSpy spy(&b, &AddAccountButton::addRequested);
        QTest::mouseClick(&b, Qt::LeftButton);
        QCOMPARE(spy.size(), 1);
        QCOMPARE(spy.takeFirst().first().toBool(), false);
        QTest::mouseClick(&b, Qt::RightButton);
        QCOMPARE(spy.size(), 0);
        QTest::mouseClick(&b, Qt::RightButton, Qt::ShiftModifier);
        QCOMPARE(spy.size(), 0);
        QTest::mouseClick(&b, Qt::RightButton, Qt::ShiftModifier | Qt::AltModifier);
        QCOMPARE(spy.size(), 1);
        QCOMPARE(spy.takeFirst().first().toBool(), true);
    }
    void loginModes() {
        Accounts accounts(nullptr, true);
        AuthDialog d(&accounts, false);
        d.show();
        auto email = d.findChild<QLineEdit *>("loginEmail");
        auto password = d.findChild<QLineEdit *>("loginPassword");
        QVERIFY(email);
        QVERIFY(password);
        QCOMPARE(password->echoMode(), QLineEdit::Password);
        auto mode = d.findChild<QComboBox *>();
        QVERIFY(mode);
        for (int i = 0; i < mode->count(); ++i) {
            mode->setCurrentIndex(i);
            QTest::qWait(5);
            QVERIFY(email->isVisible());
            QCOMPARE(password->isVisible(), i < 2);
        }
    }
    void allOperationForms() {
        ApiClient api(QUrl("https://api.monnettsocial.com"));
        OperationPanel panel(&api, false);
        panel.resize(1100, 800);
        panel.show();
        auto list = panel.findChild<QListWidget *>();
        QVERIFY(list);
        QCOMPARE(list->count(), 167);
        for (int i = 0; i < list->count(); ++i) {
            list->setCurrentRow(i);
            QVERIFY(!list->item(i)->text().contains("/admin/"));
            QVERIFY(!list->item(i)->text().contains("/webhook/"));
        }
        auto bodies = panel.findChildren<QPlainTextEdit *>();
        QCOMPARE(bodies.size(), 3);
    }

    void nativeScreensAndPublishing() {
        const QJsonObject user{
            {"id", 1}, {"publicId", "user-uuid"}, {"username", "fixture"}, {"name", "Fixture User"}};
        const QJsonObject post{
            {"id", 42}, {"publicId", "post-uuid"}, {"description", "fixture-post"}, {"posted_by", user}};
        MockServer server;
        server.handler = [user, post](const Captured &request) {
            const auto path = QUrl(QString::fromUtf8(request.target)).path();
            QJsonValue value = QJsonArray{};
            if (path == "/api/v1/app/feed")
                value = QJsonObject{{"timeline", QJsonArray{QJsonObject{{"date_key", "today"},
                                                                        {"posts", QJsonArray{post}}}}}};
            else if (path == "/api/v1/app/feed/discover")
                value = QJsonObject{{"posts", QJsonArray{post}}};
            else if (path == "/api/v1/app/stories")
                value = QJsonObject{{"users", QJsonArray{QJsonObject{{"username", "fixture"},
                                                                     {"stories", QJsonArray{post}}}}}};
            else if (path == "/api/profile/1" || path == "/api/user")
                value = QJsonObject{{"user", user}};
            else if (path == "/api/users/1/posts")
                value = QJsonObject{{"data", QJsonArray{post}}};
            else if (path == "/api/v1/app/post/create")
                value = "created-post-uuid";
            else if (path == "/api/v1/app/post/status")
                value = QJsonArray{QJsonObject{{"postId", "created-post-uuid"}, {"status", "APPROVED"}}};
            else if (path == "/api/v1/app/entitlements/quotas")
                value = QJsonObject{{"tier", "FREE"}, {"quotas", QJsonObject{}}};
            else if (path == "/api/notification-count")
                value = QJsonObject{{"count", 0}};
            else if (path == "/api/v1/app/msg/session")
                return qMakePair(200, QJsonValue(QJsonObject{{"success", false},
                                                             {"message", "Fixture: no Matrix session"}}));
            return qMakePair(200, QJsonValue(QJsonObject{{"success", true}, {"payload", value}}));
        };
        Window window(nullptr, true);
        QSignalBlocker block(&window.m_accounts);
        auto key =
            window.m_accounts.add({{"jwt", "fixture-token"}, {"user", user}}, "fixture@example.org", false);
        auto client = new ApiClient(&window.m_accounts, key, &window);
        client->setTestBase(server.base());
        window.m_clients.insert(key, client);
        block.unblock();
        window.accountChanged();
        window.show();
        auto containsText = [](QWidget *root, const QString &needle) {
            for (auto label : root->findChildren<QLabel *>())
                if (label->text().contains(needle))
                    return true;
            return false;
        };
        QTRY_VERIFY(containsText(window.m_page, "fixture-post"));
        for (int i = 1; i < 12; ++i) {
            window.navigate(i);
            QTest::qWait(35);
            QVERIFY(window.m_page);
            QVERIFY(window.m_nav[i]->isChecked());
        }
        window.navigate(4);
        QTRY_VERIFY(containsText(window.m_page, "Fixture: no Matrix session"));
        window.navigate(10);
        bool security = false;
        for (auto b : window.m_page->findChildren<QPushButton *>())
            security |= b->text().contains("Безопасность:");
        QVERIFY(security);
        ComposeDialog compose(client, {}, &window);
        compose.show();
        auto input = compose.findChildren<QPlainTextEdit *>().first();
        input->setPlainText("fixture publication");
        auto publish = compose.findChild<QPushButton *>("primary");
        QVERIFY(publish);
        QTest::mouseClick(publish, Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(containsText(&compose, "Публикация готова"), 5000);
        int creates = 0;
        for (const auto &r : server.requests)
            if (r.target == "/api/v1/app/post/create") {
                ++creates;
                auto body = Json::parse(r.body).toObject();
                QCOMPARE(body["post_type"].toString(), "text");
                QCOMPARE(body["description"].toString(), "fixture publication");
                QCOMPARE(body["media_count"].toInt(), 0);
            }
        QCOMPARE(creates, 1);
        compose.hide();
        CommentsDialog comments(client, post, &window);
        comments.show();
        comments.findChild<QPlainTextEdit *>()->setPlainText("fixture comment");
        QTest::mouseClick(comments.findChild<QPushButton *>("primary"), Qt::LeftButton);
        QTRY_VERIFY(std::any_of(server.requests.begin(), server.requests.end(), [](const Captured &r) {
            return r.target == "/api/v1/app/comments" && r.method == "POST";
        }));
        for (const auto &r : server.requests)
            if (r.target == "/api/v1/app/comments" && r.method == "POST")
                QCOMPARE(Json::parse(r.body).toObject()["post_id"].toString(), "post-uuid");
    }
};
QTEST_MAIN(WidgetTest)
#include "widget_test.moc"
