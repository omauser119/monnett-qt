#pragma once
#include <QJsonObject>
#include <QObject>
#include <QUrl>
#include <QVector>
struct Account {
    QString key, email, label, jwt, refresh, deviceId;
    bool development = false;
    QJsonObject user;
    QUrl baseUrl() const {
        return QUrl(development ? "https://api-dev.monnettsocial.com" : "https://api.monnettsocial.com");
    }
};
class Accounts : public QObject {
    Q_OBJECT
  public:
    explicit Accounts(QObject *parent = nullptr, bool memoryOnly = false);
    const QVector<Account> &all() const { return m_accounts; }
    Account *find(const QString &key);
    QString activeKey() const { return m_active; }
    Account *active() { return find(m_active); }
    QString add(const QJsonObject &session, const QString &email, bool development);
    void updateSession(const QString &key, const QJsonObject &session);
    void updateUser(const QString &key, const QJsonObject &user);
    void select(const QString &key);
    void remove(const QString &key);
    bool persist();
    QString storageWarning() const { return m_warning; }
  signals:
    void changed();
    void activeChanged();
    void storageError(QString message);

  private:
    void load();
    QVector<Account> m_accounts;
    QString m_active, m_warning;
    bool m_memoryOnly;
};
