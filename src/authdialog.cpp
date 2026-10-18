#include "authdialog.h"
#include "json.h"
#include "widgets.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDesktopServices>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
AuthDialog::AuthDialog(Accounts *a, bool dev, QWidget *p) : QDialog(p), m_accounts(a), m_dev(dev) {
    setWindowTitle(dev ? "Аккаунт тестового сервера" : "Вход в Monnett");
    setMinimumWidth(460);
    auto l = new QVBoxLayout(this);
    l->setContentsMargins(30, 28, 30, 28);
    l->setSpacing(16);
    l->addWidget(Ui::label(dev ? "Тестовый сервер" : "Рады видеть вас", "title"));
    l->addWidget(Ui::label(dev ? "api-dev.monnettsocial.com · Отдельная сессия и данные"
                               : "Ваш Monnett. На большом экране.",
                           "muted"));
    m_api = new ApiClient(Account{.development = dev}.baseUrl(), this);
    m_mode = new QComboBox;
    m_mode->addItems({"Войти", "Создать аккаунт", "Подтвердить email", "Восстановить пароль"});
    l->addWidget(m_mode);
    auto f = new QFormLayout;
    l->addLayout(f);
    m_email = new QLineEdit;
    m_email->setPlaceholderText("you@example.com");
    m_email->setObjectName("loginEmail");
    m_password = new QLineEdit;
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setObjectName("loginPassword");
    m_name = new QLineEdit;
    m_birth = new QDateEdit(QDate(2000, 1, 1));
    m_birth->setCalendarPopup(true);
    m_birth->setDisplayFormat("yyyy-MM-dd");
    m_referral = new QLineEdit;
    m_otp = new QLineEdit;
    m_otp->setPlaceholderText("Код из письма");
    m_newPassword = new QLineEdit;
    m_newPassword->setEchoMode(QLineEdit::Password);
    m_terms = new QCheckBox("Я принимаю условия сервиса Monnett");
    f->addRow("Email", m_email);
    f->addRow("Пароль", m_password);
    f->addRow("Имя", m_name);
    f->addRow("Дата рождения", m_birth);
    f->addRow("Код приглашения", m_referral);
    f->addRow("Код подтверждения", m_otp);
    f->addRow("Новый пароль", m_newPassword);
    f->addRow(m_terms);
    m_status = Ui::label({}, "muted");
    l->addWidget(m_status);
    m_submit = Ui::button("Войти", [this] { submit(); }, this, "primary");
    l->addWidget(m_submit);
    auto send = Ui::button("Отправить код на email", [this] {
        if (m_email->text().trimmed().isEmpty()) {
            m_status->setText("Введите email");
            return;
        }
        call("/api/send-otp",
             {{"email", m_email->text().trimmed()},
              {"type", m_mode->currentIndex() == 3 ? "reset-password" : "register"}},
             [this](const ApiResult &r) {
                 m_status->setText(r.ok ? "Код отправлен. Проверьте почту." : r.error);
             });
    });
    l->addWidget(send);
    l->addWidget(Ui::button("Условия сервиса ↗",
                            [] { QDesktopServices::openUrl(QUrl("https://monnettsocial.com")); }));
    connect(m_mode, &QComboBox::currentIndexChanged, this, &AuthDialog::modeChanged);
    connect(m_password, &QLineEdit::returnPressed, this, &AuthDialog::submit);
    modeChanged();
}
void AuthDialog::modeChanged() {
    auto i = m_mode->currentIndex();
    m_password->setVisible(i < 2);
    m_name->setVisible(i == 1);
    m_birth->setVisible(i == 1);
    m_referral->setVisible(i == 1);
    m_terms->setVisible(i == 1);
    m_otp->setVisible(i >= 2);
    m_newPassword->setVisible(i == 3);
    auto form = qobject_cast<QFormLayout *>(layout()->itemAt(3)->layout());
    if (form) {
        form->setRowVisible(m_password, i < 2);
        form->setRowVisible(m_name, i == 1);
        form->setRowVisible(m_birth, i == 1);
        form->setRowVisible(m_referral, i == 1);
        form->setRowVisible(m_terms, i == 1);
        form->setRowVisible(m_otp, i >= 2);
        form->setRowVisible(m_newPassword, i == 3);
    }
    m_submit->setText(
        QStringList{"Войти", "Зарегистрироваться", "Подтвердить и войти", "Изменить пароль"}[i]);
    m_status->clear();
}
void AuthDialog::call(const QString &p, const QJsonObject &b, ApiCallback cb) {
    m_submit->setEnabled(false);
    m_status->setText("Подключение…");
    m_api->request("POST", p, {}, b, this, [this, cb](const ApiResult &r) {
        m_submit->setEnabled(true);
        cb(r);
    });
}
void AuthDialog::submit() {
    auto email = m_email->text().trimmed();
    auto mode = m_mode->currentIndex();
    if (email.isEmpty()) {
        m_status->setText("Введите email");
        return;
    }
    auto acceptSession = [this, email](const ApiResult &r) {
        if (!r.ok) {
            m_status->setText(r.error);
            return;
        }
        auto s = r.data.toObject();
        if (s["jwt"].toString().isEmpty()) {
            m_status->setText("В ответе нет токена сессии. Подтвердите email.");
            return;
        }
        m_accounts->add(s, email, m_dev);
        m_password->clear();
        accept();
    };
    if (mode == 0) {
        if (m_password->text().isEmpty()) {
            m_status->setText("Введите пароль");
            return;
        }
        call("/api/auth/login", {{"email", email}, {"password", m_password->text()}}, acceptSession);
    } else if (mode == 1) {
        if (!m_terms->isChecked() || m_password->text().isEmpty()) {
            m_status->setText("Введите пароль и примите условия сервиса");
            return;
        }
        QJsonObject b{{"email", email},
                      {"password", m_password->text()},
                      {"name", m_name->text()},
                      {"date_of_birth", m_birth->date().toString(Qt::ISODate)},
                      {"tos_accepted", true}};
        if (!m_referral->text().trimmed().isEmpty())
            b["referral_code"] = m_referral->text().trimmed();
        call("/api/auth/register", b, [this, email](const ApiResult &r) {
            if (!r.ok) {
                m_status->setText(r.error);
                return;
            }
            m_mode->setCurrentIndex(2);
            call("/api/send-otp", {{"email", email}, {"type", "register"}}, [this](const ApiResult &otp) {
                m_status->setText(otp.ok ? "Аккаунт создан. Введите код из письма." : otp.error);
            });
        });
    } else if (mode == 2) {
        call("/api/verify-otp", {{"email", email}, {"otp", m_otp->text().trimmed()}, {"type", "register"}},
             acceptSession);
    } else {
        if (m_otp->text().trimmed().isEmpty() || m_newPassword->text().isEmpty()) {
            m_status->setText("Введите код и новый пароль");
            return;
        }
        call("/api/verify-otp",
             {{"email", email}, {"otp", m_otp->text().trimmed()}, {"type", "reset-password"}},
             [this](const ApiResult &r) {
                 if (!r.ok) {
                     m_status->setText(r.error);
                     return;
                 }
                 auto token = r.data.toObject()["reset_token"].toString();
                 if (token.isEmpty()) {
                     m_status->setText("Сервер не вернул reset_token");
                     return;
                 }
                 call("/api/reset-password",
                      {{"reset_token", token}, {"new_password", m_newPassword->text()}},
                      [this](const ApiResult &reset) {
                          if (!reset.ok) {
                              m_status->setText(reset.error);
                              return;
                          }
                          m_newPassword->clear();
                          m_password->clear();
                          m_mode->setCurrentIndex(0);
                          m_status->setText("Пароль изменён. Теперь войдите.");
                      });
             });
    }
}
