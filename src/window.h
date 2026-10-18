#pragma once
#include "api.h"
#include <QHash>
#include <QMainWindow>
class QVBoxLayout;
class QHBoxLayout;
class QStackedWidget;
class QScrollArea;
class QLineEdit;
class QLabel;
class QPushButton;
class MessagesPage;
class Window : public QMainWindow {
    friend class WidgetTest;
    Q_OBJECT
  public:
    explicit Window(QWidget *parent = nullptr, bool preview = false);
    void showPreview();

  private:
    ApiClient *api();
    void buildSidebar();
    void accountsChanged();
    void accountChanged();
    void addAccount(bool development);
    void navigate(int page);
    void newPage(const QString &title, const QString &subtitle);
    void refresh();
    void feed(bool more = false);
    void stories(bool more = false);
    void search();
    void notifications(bool more = false);
    void profile(const QJsonObject &user = {});
    void people();
    void media();
    void plans();
    void settings();
    void renderPost(const QJsonObject &post, QVBoxLayout *target, ApiClient *client, QWidget *context,
                    bool story = false);
    void renderUser(const QJsonObject &user, QVBoxLayout *target, QWidget *context);
    void renderList(const QJsonValue &data, QVBoxLayout *target, QWidget *context, const QString &kind = {});
    void comments(const QJsonObject &post);
    void compose();
    void editProfile();
    void operation(const QString &match = {});
    void showStory(const QJsonObject &story, const QJsonObject &user);
    void setStatus(const QString &text, bool error = false);
    void failure(const ApiResult &r);
    void pageRequest(const QString &path, const QUrlQuery &query,
                     std::function<void(const ApiResult &)> done);
    Accounts m_accounts;
    QHash<QString, ApiClient *> m_clients;
    QWidget *m_sidebarAccounts, *m_page = nullptr;
    QVBoxLayout *m_accountsLayout, *m_content, *m_items = nullptr;
    QHBoxLayout *m_toolbar;
    QScrollArea *m_scroll;
    QLabel *m_title, *m_subtitle, *m_status, *m_environment;
    QVector<QPushButton *> m_nav;
    QPushButton *m_more = nullptr;
    int m_current = 0, m_number = 1;
    QString m_cursor, m_keyword, m_searchType = "accounts", m_notificationFilter = "all";
    bool m_busy = false;
    QJsonObject m_quotas;
    QJsonObject m_viewedProfile;
};
