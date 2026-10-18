#include "authdialog.h"
#include "operationpanel.h"
#include "widgets.h"
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QtTest>
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
};
QTEST_MAIN(WidgetTest)
#include "widget_test.moc"
