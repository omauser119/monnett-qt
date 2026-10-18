#include "window.h"
#include "authdialog.h"
#include "json.h"
#include "messages.h"
#include "operationpanel.h"
#include "panels.h"
#include "widgets.h"
#include <QApplication>
#include <QAudioOutput>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QMediaPlayer>
#include <QMenu>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QScrollArea>
#include <QSlider>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QVideoWidget>
namespace {
const QStringList names = {"Лента",       "Обзор", "Истории",   "Поиск",    "Сообщения", "Уведомления",
                           "Мой профиль", "Люди",  "Мои файлы", "Подписка", "Настройки", "Методы API"};
const QStringList symbols = {"⌂", "◎", "◉", "⌕", "✉", "♧", "◯", "♧", "▧", "◇", "⚙", "⌘"};
QString seg(const QString &s) {
    return ApiClient::segment(s);
}
QString prettyTime(const QJsonObject &o) {
    auto s = Json::text(o, {"createdAt", "created_at", "publishedAt", "date"});
    auto d = QDateTime::fromString(s, Qt::ISODate);
    return d.isValid() ? d.toLocalTime().toString("dd MMM yyyy · HH:mm") : s;
}
QDialog *dialog(QWidget *p, const QString &title, int w = 720, int h = 680) {
    auto d = new QDialog(p);
    d->setAttribute(Qt::WA_DeleteOnClose);
    d->setWindowTitle(title);
    d->resize(w, h);
    return d;
}
void play(QWidget *p, const QUrl &url) {
    if (url.scheme() != "http" && url.scheme() != "https")
        return;
    auto d = dialog(p, "Медиа", 800, 600);
    auto l = new QVBoxLayout(d);
    auto video = new QVideoWidget;
    l->addWidget(video, 1);
    auto player = new QMediaPlayer(d);
    auto audio = new QAudioOutput(d);
    audio->setVolume(0.7);
    player->setAudioOutput(audio);
    player->setVideoOutput(video);
    auto controls = new QHBoxLayout;
    l->addLayout(controls);
    controls->addWidget(Ui::button("▶ / Ⅱ", [player] {
        if (player->playbackState() == QMediaPlayer::PlayingState)
            player->pause();
        else
            player->play();
    }));
    auto slider = new QSlider(Qt::Horizontal);
    controls->addWidget(slider, 1);
    QObject::connect(player, &QMediaPlayer::durationChanged, slider,
                     [slider](qint64 n) { slider->setRange(0, int(n)); });
    QObject::connect(player, &QMediaPlayer::positionChanged, slider, [slider](qint64 n) {
        if (!slider->isSliderDown())
            slider->setValue(int(n));
    });
    QObject::connect(slider, &QSlider::sliderMoved, player, &QMediaPlayer::setPosition);
    auto status = Ui::label({}, "muted");
    l->addWidget(status);
    QObject::connect(player, &QMediaPlayer::errorOccurred, status,
                     [status, player] { status->setText(player->errorString()); });
    player->setSource(url);
    d->show();
    player->play();
}
} // namespace
Window::Window(QWidget *p, bool preview) : QMainWindow(p), m_accounts(this, preview) {
    setWindowTitle("Monnett — Linux");
    setWindowIcon(QIcon(":/monnett.svg"));
    resize(1280, 900);
    setMinimumSize(980, 680);
    auto root = new QWidget;
    setCentralWidget(root);
    auto h = new QHBoxLayout(root);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(0);
    auto sidebar = new QFrame;
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(248);
    auto sl = new QVBoxLayout(sidebar);
    sl->setContentsMargins(20, 29, 20, 22);
    sl->setSpacing(5);
    sl->addWidget(Ui::label("monnett", "brand"));
    sl->addWidget(Ui::label("YOUR PEOPLE. YOUR WORLD.", "eyebrow"));
    sl->addSpacing(20);
    auto navigation = new QWidget;
    auto navigationLayout = new QVBoxLayout(navigation);
    navigationLayout->setContentsMargins(0, 0, 0, 0);
    navigationLayout->setSpacing(4);
    for (int i = 0; i < names.size(); ++i) {
        auto b = Ui::button(symbols[i] + "   " + names[i], [this, i] { navigate(i); }, sidebar, "nav");
        b->setCheckable(true);
        m_nav.append(b);
        navigationLayout->addWidget(b);
    }
    navigationLayout->addStretch();
    auto navigationScroll = new QScrollArea;
    navigationScroll->setWidgetResizable(true);
    navigationScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    navigationScroll->setWidget(navigation);
    sl->addWidget(navigationScroll, 1);
    sl->addWidget(Ui::label("АККАУНТЫ", "eyebrow"));
    m_sidebarAccounts = new QWidget;
    m_accountsLayout = new QVBoxLayout(m_sidebarAccounts);
    m_accountsLayout->setContentsMargins(0, 8, 0, 8);
    m_accountsLayout->setSpacing(6);
    auto ascroll = new QScrollArea;
    ascroll->setWidgetResizable(true);
    ascroll->setWidget(m_sidebarAccounts);
    ascroll->setMaximumHeight(120);
    ascroll->setMinimumHeight(40);
    sl->addWidget(ascroll);
    auto add = new AddAccountButton;
    sl->addWidget(add);
    connect(add, &AddAccountButton::addRequested, this, &Window::addAccount);
    sl->addSpacing(8);
    sl->addWidget(Ui::label("Независимый клиент · v0.1.0", "muted"));
    h->addWidget(sidebar);
    auto main = new QWidget;
    auto ml = new QVBoxLayout(main);
    ml->setContentsMargins(36, 30, 36, 18);
    ml->setSpacing(17);
    auto top = new QHBoxLayout;
    auto text = new QVBoxLayout;
    m_title = Ui::label({}, "title");
    m_subtitle = Ui::label({}, "subtitle");
    text->addWidget(m_title);
    text->addWidget(m_subtitle);
    top->addLayout(text, 1);
    m_environment = Ui::label("MONNETT", "badge");
    m_environment->setFixedHeight(32);
    top->addWidget(m_environment, 0, Qt::AlignVCenter);
    top->addSpacing(12);
    top->addWidget(Ui::button("↻", [this] { refresh(); }));
    top->addWidget(Ui::button("＋  Публикация", [this] { compose(); }, main, "primary"));
    ml->addLayout(top);
    m_toolbar = new QHBoxLayout;
    ml->addLayout(m_toolbar);
    m_status = Ui::label({}, "muted");
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    ml->addWidget(m_status);
    m_scroll = new QScrollArea;
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ml->addWidget(m_scroll, 1);
    h->addWidget(main, 1);
    connect(&m_accounts, &Accounts::changed, this, &Window::accountsChanged);
    connect(&m_accounts, &Accounts::activeChanged, this, &Window::accountChanged);
    connect(&m_accounts, &Accounts::storageError, this, [this](const QString &e) { setStatus(e, true); });
    accountsChanged();
    navigate(0);
    if (!m_accounts.storageWarning().isEmpty())
        setStatus(m_accounts.storageWarning(), true);
}
ApiClient *Window::api() {
    auto a = m_accounts.active();
    if (!a)
        return nullptr;
    if (!m_clients.contains(a->key)) {
        auto client = new ApiClient(&m_accounts, a->key, this);
        m_clients[a->key] = client;
        connect(client, &ApiClient::sessionExpired, this, [this, client] {
            if (m_accounts.activeKey() == client->accountKey())
                setStatus("Сессия истекла. Нажмите «Добавить аккаунт» и войдите заново.", true);
        });
    }
    return m_clients.value(a->key);
}
void Window::accountsChanged() {
    Ui::clear(m_accountsLayout);
    for (const auto &a : m_accounts.all()) {
        auto b = Ui::button((a.development ? "[DEV] " : "@") + (a.label.isEmpty() ? a.email : a.label),
                            [this, key = a.key] { m_accounts.select(key); }, m_sidebarAccounts, "account");
        b->setCheckable(true);
        b->setChecked(a.key == m_accounts.activeKey());
        m_accountsLayout->addWidget(b);
    }
}
void Window::accountChanged() {
    accountsChanged();
    m_quotas = {};
    m_viewedProfile = {};
    if (auto a = m_accounts.active())
        m_environment->setText(a->development ? "DEV · ТЕСТОВЫЙ" : "MONNETT · LIVE");
    else
        m_environment->setText("MONNETT");
    navigate(m_current);
}
void Window::addAccount(bool dev) {
    AuthDialog d(&m_accounts, dev, this);
    d.exec();
}
void Window::newPage(const QString &t, const QString &s) {
    Ui::clear(m_toolbar);
    if (auto old = m_scroll->takeWidget()) {
        old->hide();
        old->deleteLater();
    }
    m_page = new QWidget;
    m_content = new QVBoxLayout(m_page);
    m_content->setContentsMargins(0, 0, 10, 0);
    m_content->setSpacing(18);
    m_content->setAlignment(Qt::AlignTop);
    m_scroll->setWidget(m_page);
    m_title->setText(t);
    m_subtitle->setText(s);
    m_status->clear();
    m_items = nullptr;
    m_more = nullptr;
    m_number = 1;
    m_cursor.clear();
    m_busy = false;
}
void Window::navigate(int i) {
    m_current = i;
    for (int n = 0; n < m_nav.size(); ++n)
        m_nav[n]->setChecked(n == i);
    newPage(names.value(i), "Ваше пространство в Monnett");
    auto a = m_accounts.active();
    if (!a) {
        QVBoxLayout *l;
        auto c = Ui::card(&l, m_page);
        l->addSpacing(35);
        l->addWidget(Ui::label("Ближе к своим людям.", "title"));
        l->addWidget(Ui::label("Лента, истории, сообщения и ваши сообщества — в одном приложении для Linux.",
                               "postText"));
        l->addSpacing(16);
        l->addWidget(Ui::button("Войти в Monnett", [this] { addAccount(false); }, c, "primary"));
        l->addWidget(
            Ui::label("Добавляйте несколько аккаунтов и переключайтесь между ними в левом меню.", "muted"));
        l->addSpacing(35);
        m_content->addWidget(c);
        return;
    }
    m_environment->setText(a->development ? "DEV · ТЕСТОВЫЙ" : "MONNETT · LIVE");
    if (a->jwt.isEmpty()) {
        m_content->addWidget(Ui::label(
            "Войдите заново через «Добавить аккаунт». Метаданные аккаунта сохранены, токен недоступен.",
            "postText"));
        return;
    }
    switch (i) {
    case 0:
    case 1:
        feed();
        break;
    case 2:
        stories();
        break;
    case 3: {
        auto edit = new QLineEdit;
        edit->setPlaceholderText("Имя пользователя, текст или #тег");
        edit->setText(m_keyword);
        auto type = new QComboBox;
        type->addItem("Люди", "accounts");
        type->addItem("Публикации", "posts");
        type->addItem("Хэштеги", "tags");
        type->setCurrentIndex(qMax(0, type->findData(m_searchType)));
        m_toolbar->addWidget(edit, 1);
        m_toolbar->addWidget(type);
        auto run = [this, edit, type] {
            m_keyword = edit->text().trimmed();
            m_searchType = type->currentData().toString();
            m_number = 1;
            search();
        };
        m_toolbar->addWidget(Ui::button("Найти", run));
        connect(edit, &QLineEdit::returnPressed, m_page, run);
        m_items = new QVBoxLayout;
        m_items->setSpacing(16);
        m_content->addLayout(m_items);
        if (!m_keyword.isEmpty())
            search();
        else
            m_items->addWidget(Ui::label("Найдите людей и идеи, которые вам близки.", "muted"));
        break;
    }
    case 4:
        m_content->addWidget(new MessagesPage(api(), &m_accounts, a->key, m_page));
        break;
    case 5:
        notifications();
        break;
    case 6:
        profile();
        break;
    case 7:
        people();
        break;
    case 8:
        media();
        break;
    case 9:
        plans();
        break;
    case 10:
        settings();
        break;
    case 11: {
        auto op = new OperationPanel(api(), a->development, m_page);
        op->setMinimumHeight(610);
        m_content->addWidget(op);
        break;
    }
    }
}
void Window::refresh() {
    navigate(m_current);
}
void Window::setStatus(const QString &t, bool error) {
    m_status->setStyleSheet(error ? "color:#ff7b95" : "color:#94949f");
    m_status->setText(t);
}
void Window::failure(const ApiResult &r) {
    setStatus(QString("HTTP %1 · %2").arg(r.status).arg(r.error), true);
}
void Window::pageRequest(const QString &p, const QUrlQuery &q, std::function<void(const ApiResult &)> done) {
    auto client = api();
    if (!client)
        return;
    setStatus("Загрузка…");
    client->get(
        p, m_page,
        [this, done](const ApiResult &r) {
            setStatus({});
            if (!r.ok) {
                failure(r);
                return;
            }
            done(r);
        },
        q);
}
void Window::feed(bool more) {
    bool discover = m_current == 1;
    if (!more) {
        m_subtitle->setText(discover ? "Новые люди. Новые точки зрения."
                                     : "Свежие моменты от тех, на кого вы подписаны.");
        m_items = new QVBoxLayout;
        m_items->setSpacing(18);
        m_content->addLayout(m_items);
        if (discover) {
            auto scope = new QComboBox;
            scope->addItems({"GLOBAL", "COUNTRY", "CITY"});
            auto city = new QLineEdit;
            city->setPlaceholderText("Город");
            auto country = new QLineEdit;
            country->setPlaceholderText("Страна");
            m_toolbar->addWidget(scope);
            m_toolbar->addWidget(city);
            m_toolbar->addWidget(country);
            auto apply = [this, scope, city, country] {
                m_page->setProperty("scope", scope->currentText());
                m_page->setProperty("city", city->text().trimmed());
                m_page->setProperty("country", country->text().trimmed());
                Ui::clear(m_items);
                m_number = 1;
                m_cursor.clear();
                feed(true);
            };
            m_toolbar->addWidget(Ui::button("Применить", apply));
        }
        m_more = Ui::button("Загрузить ещё", [this] {
            ++m_number;
            feed(true);
        });
        m_content->addWidget(m_more);
    }
    if (m_busy)
        return;
    m_busy = true;
    m_more->setEnabled(false);
    QUrlQuery q;
    q.addQueryItem("page", QString::number(m_number));
    q.addQueryItem("limit", "30");
    if (discover) {
        if (!m_cursor.isEmpty())
            q.addQueryItem("cursor", m_cursor);
        q.addQueryItem("startScope", m_page->property("scope").toString().isEmpty()
                                         ? "GLOBAL"
                                         : m_page->property("scope").toString());
        q.addQueryItem("includeUnlocated", "true");
        if (!m_page->property("city").toString().isEmpty())
            q.addQueryItem("city", m_page->property("city").toString());
        if (!m_page->property("country").toString().isEmpty())
            q.addQueryItem("country", m_page->property("country").toString());
    }
    auto client = api();
    setStatus("Загрузка ленты…");
    client->get(
        discover ? "/api/v1/app/feed/discover" : "/api/v1/app/feed", m_page,
        [this, client](const ApiResult &r) {
            m_busy = false;
            m_more->setEnabled(true);
            if (!r.ok) {
                failure(r);
                if (m_number > 1)
                    --m_number;
                return;
            }
            setStatus({});
            auto posts = Json::posts(r.data);
            for (auto p : posts)
                renderPost(p.toObject(), m_items, client, m_page);
            auto o = r.data.toObject();
            m_cursor = o["next_cursor"].toString();
            auto pg = o["meta"].toObject()["pagination"].toObject();
            bool last = posts.isEmpty() || (pg.contains("pageCount") && m_number >= pg["pageCount"].toInt());
            m_more->setVisible(!last);
            if (posts.isEmpty() && m_number == 1)
                m_items->addWidget(
                    Ui::label("Пока нет публикаций. Найдите друзей во вкладке «Поиск».", "postText"));
        },
        q);
}
void Window::renderPost(const QJsonObject &p, QVBoxLayout *target, ApiClient *client, QWidget *context,
                        bool story) {
    QVBoxLayout *l;
    auto c = Ui::card(&l, context);
    auto author = Json::object(p.value("posted_by"), {"user"});
    if (author.isEmpty())
        author = p["user"].toObject();
    auto head = new QHBoxLayout;
    auto profileButton = Ui::button(Ui::userName(author), [this, author] { profile(author); }, c);
    head->addWidget(profileButton);
    head->addStretch();
    head->addWidget(Ui::label(prettyTime(p), "muted"));
    l->addLayout(head);
    auto title = Json::text(p, {"title"});
    if (!title.isEmpty())
        l->addWidget(Ui::label(title, "sectionTitle"));
    auto desc = Json::text(p, {"description", "repost_caption"});
    if (!desc.isEmpty()) {
        auto text = Ui::label(desc, "postText");
        text->setTextInteractionFlags(Qt::TextSelectableByMouse);
        l->addWidget(text);
    }
    for (auto value : p["media"].toArray()) {
        auto media = value.toObject();
        auto url = Ui::mediaUrl(media);
        auto mime = media["mime"].toString();
        if (url.isRelative())
            url = client->baseUrl().resolved(url);
        if (url.isEmpty())
            continue;
        if (mime.startsWith("video/") || mime.startsWith("audio/"))
            l->addWidget(Ui::button("▶  Воспроизвести медиа", [this, url] { play(this, url); }, c));
        else
            l->addWidget(new RemoteImage(url, c));
    }
    auto link = QUrl(p["link"].toString());
    if (link.scheme() == "https" || link.scheme() == "http")
        l->addWidget(Ui::button("↗  " + link.toDisplayString(), [link] { QDesktopServices::openUrl(link); }));
    auto row = new QHBoxLayout;
    l->addLayout(row);
    auto publicId = Json::id(p);
    auto likes = Json::text(p, {"likes_count", "like_count"}, "0");
    auto like = new QPushButton((p["is_liked"].toBool() ? "♥  " : "♡  ") + likes);
    like->setCheckable(true);
    like->setChecked(p["is_liked"].toBool());
    row->addWidget(like);
    connect(like, &QPushButton::clicked, context, [=, this](bool checked) {
        like->setEnabled(false);
        QString path = story ? "/api/like" : "/api/v1/app/posts/" + seg(publicId) + "/like";
        client->request(story || checked ? "POST" : "DELETE", path, {},
                        story ? QJsonValue(QJsonObject{{"post_id", p.value("id")}}) : QJsonValue(), like,
                        [this, like, checked](const ApiResult &r) {
                            like->setEnabled(true);
                            if (!r.ok) {
                                like->setChecked(!checked);
                                Ui::error(this, r.error);
                                return;
                            }
                            auto o = r.data.toObject();
                            auto n = Json::text(o, {"likeCount", "likes_count"});
                            like->setText((checked ? "♥  " : "♡  ") + (n.isEmpty() ? QString("Готово") : n));
                        });
    });
    row->addWidget(Ui::button(
        "☰  " + Json::text(p, {"comments_count", "comment_count"}, "0"), [this, p] { comments(p); }, c));
    row->addStretch();
    row->addWidget(Ui::button(
        "↗",
        [this, p] {
            auto a = m_accounts.active();
            QString base =
                a && a->development ? "https://share-dev.monnett.social" : "https://share.monnett.social";
            QApplication::clipboard()->setText(base + "/post/" + Json::id(p));
            setStatus("Ссылка скопирована");
        },
        c));
    auto more = new QPushButton("•••");
    row->addWidget(more);
    connect(more, &QPushButton::clicked, context, [=, this] {
        QMenu menu;
        menu.addAction("Лайкнувшие", [this, publicId, client] {
            auto d = dialog(this, "Лайкнувшие");
            auto layout = new QVBoxLayout(d);
            client->get("/api/v1/app/posts/" + seg(publicId) + "/likes", d,
                        [this, d, layout](const ApiResult &r) {
                            if (!r.ok) {
                                layout->addWidget(Ui::label(r.error));
                                return;
                            }
                            renderList(r.data, layout, d, "users");
                        });
            d->show();
        });
        menu.addAction("Репост", [this, p, client, c] {
            bool ok = false;
            auto caption = QInputDialog::getMultiLineText(this, "Репост", "Ваш комментарий", {}, &ok);
            if (!ok)
                return;
            client->request(
                "POST", "/api/posts", {},
                QJsonObject{{"post_type", "post"}, {"repost_of", p.value("id")}, {"repost_caption", caption}},
                c, [this](const ApiResult &r) {
                    if (!r.ok)
                        Ui::error(this, r.error);
                    else
                        setStatus("Репост опубликован");
                });
        });
        bool own = false;
        if (auto a = m_accounts.active())
            own = Json::id(author) == Json::id(a->user) && !Json::id(author).isEmpty();
        if (own) {
            menu.addAction("Изменить текст", [this, p, client, c, publicId] {
                bool ok;
                auto text = QInputDialog::getMultiLineText(this, "Редактировать", "Текст",
                                                           p["description"].toString(), &ok);
                if (!ok)
                    return;
                client->request("PUT", "/api/v1/app/post/" + seg(publicId), {},
                                QJsonObject{{"description", text}}, c, [this](const ApiResult &r) {
                                    if (!r.ok)
                                        Ui::error(this, r.error);
                                    else
                                        refresh();
                                });
            });
            menu.addAction("Удалить", [this, client, c, publicId] {
                if (QMessageBox::question(this, "Удалить публикацию?",
                                          "Это действие удалит публикацию на сервере.") != QMessageBox::Yes)
                    return;
                client->request("DELETE", "/api/v1/app/post/" + seg(publicId), {}, {}, c,
                                [this, c](const ApiResult &r) {
                                    if (!r.ok)
                                        Ui::error(this, r.error);
                                    else
                                        c->hide();
                                });
            });
        }
        menu.addAction("Пожаловаться", [this, p, client, c] {
            bool ok;
            auto reason = QInputDialog::getText(this, "Жалоба", "Причина", QLineEdit::Normal, {}, &ok);
            if (!ok || reason.trimmed().isEmpty())
                return;
            client->request("POST", "/api/reports", {},
                            QJsonObject{{"post_public_id", Json::id(p)}, {"reason", reason}}, c,
                            [this](const ApiResult &r) {
                                if (!r.ok)
                                    Ui::error(this, r.error);
                                else
                                    setStatus("Жалоба отправлена");
                            });
        });
        menu.addAction("Данные публикации", [this, p] { Ui::json(this, "Публикация", p); });
        menu.exec(more->mapToGlobal(QPoint(0, more->height())));
    });
    target->addWidget(c);
}
void Window::search() {
    if (m_keyword.isEmpty())
        return;
    Ui::clear(m_items);
    auto query = m_keyword;
    QUrlQuery q;
    q.addQueryItem("page", QString::number(m_number));
    q.addQueryItem("pageSize", "30");
    QString path = "/api/search";
    if (m_searchType == "tags") {
        path = "/api/v1/app/search/posts-by-tag";
        if (query.startsWith('#'))
            query.remove(0, 1);
        q.addQueryItem("tag", query);
        q.addQueryItem("limit", "30");
    } else {
        q.addQueryItem("type", m_searchType);
        q.addQueryItem("keyword", query);
    }
    auto client = api();
    pageRequest(path, q, [this, client](const ApiResult &r) {
        auto arr = m_searchType == "accounts" ? Json::list(r.data) : Json::posts(r.data);
        if (arr.isEmpty())
            m_items->addWidget(Ui::label("Ничего не найдено", "postText"));
        for (auto v : arr)
            if (m_searchType == "accounts")
                renderUser(v.toObject(), m_items, m_page);
            else
                renderPost(v.toObject(), m_items, client, m_page);
        auto row = new QHBoxLayout;
        row->addWidget(Ui::button("← Назад", [this] {
            if (m_number > 1) {
                --m_number;
                search();
            }
        }));
        row->addWidget(Ui::label(QString("Страница %1").arg(m_number), "muted"));
        row->addWidget(Ui::button("Далее →", [this] {
            ++m_number;
            search();
        }));
        m_items->addLayout(row);
    });
}
void Window::renderUser(const QJsonObject &user, QVBoxLayout *target, QWidget *context) {
    auto u = Json::object(user, {"user", "profile"});
    QVBoxLayout *l;
    auto c = Ui::card(&l, context);
    auto row = new QHBoxLayout;
    auto avatar = Json::object(u.value("profile_picture"));
    if (avatar.isEmpty())
        avatar = u["profilePicture"].toObject();
    auto url = Ui::mediaUrl(avatar);
    if (!url.isEmpty()) {
        auto img = new RemoteImage(url, c, 58);
        img->setFixedWidth(58);
        row->addWidget(img);
    }
    auto text = new QVBoxLayout;
    text->addWidget(Ui::label(Ui::userName(u), "sectionTitle"));
    text->addWidget(Ui::label("@" + u["username"].toString(), "muted"));
    row->addLayout(text, 1);
    row->addWidget(Ui::button("Профиль →", [this, u] { profile(u); }, c));
    l->addLayout(row);
    if (!u["bio"].toString().isEmpty())
        l->addWidget(Ui::label(u["bio"].toString()));
    target->addWidget(c);
}
void Window::renderList(const QJsonValue &data, QVBoxLayout *target, QWidget *context, const QString &kind) {
    auto array = Json::list(data);
    if (array.isEmpty()) {
        target->addWidget(Ui::label("Список пуст", "muted"));
        return;
    }
    for (auto v : array) {
        auto o = v.toObject();
        if (kind == "users" || o.contains("username") || o["user"].isObject()) {
            renderUser(o, target, context);
            continue;
        }
        QVBoxLayout *l;
        auto c = Ui::card(&l, context);
        l->addWidget(
            Ui::label(Json::text(o, {"title", "name", "message", "description", "reason", "type"}, "Запись"),
                      "sectionTitle"));
        l->addWidget(Ui::label(prettyTime(o), "muted"));
        l->addWidget(Ui::button("Подробнее", [this, o] { Ui::json(this, "Данные", o); }, c));
        target->addWidget(c);
    }
}
void Window::profile(const QJsonObject &requested) {
    auto a = m_accounts.active();
    if (!a)
        return;
    auto u = requested.isEmpty() ? a->user : requested;
    m_viewedProfile = u;
    bool own = Json::id(u) == Json::id(a->user);
    if (m_current != 6 || !requested.isEmpty()) {
        newPage(own ? "Мой профиль" : "Профиль", "Люди, с которыми хочется быть на связи");
        for (auto b : m_nav)
            b->setChecked(false);
        m_nav[6]->setChecked(own);
    }
    auto client = api();
    auto id = Json::text(u, {"id"});
    auto render = [this, client, own](QJsonObject user) {
        if (user.isEmpty()) {
            setStatus("Сервер вернул пустой профиль", true);
            return;
        }
        m_viewedProfile = user;
        QVBoxLayout *l;
        auto c = Ui::card(&l, m_page);
        auto avatar = Json::object(user.value("profilePicture"));
        if (avatar.isEmpty())
            avatar = user["profile_picture"].toObject();
        auto url = Ui::mediaUrl(avatar);
        if (!url.isEmpty())
            l->addWidget(new RemoteImage(url, c, 120));
        l->addWidget(Ui::label(Ui::userName(user), "title"));
        l->addWidget(Ui::label("@" + user["username"].toString(), "muted"));
        if (!user["bio"].toString().isEmpty())
            l->addWidget(Ui::label(user["bio"].toString(), "postText"));
        auto stats = user["stats"].toObject();
        l->addWidget(Ui::label(QString("%1 публикаций     %2 подписчиков     %3 подписок")
                                   .arg(Json::text(stats, {"postCount", "posts_count"}, "—"),
                                        Json::text(stats, {"followerCount", "followers_count"}, "—"),
                                        Json::text(stats, {"followingCount", "following_count"}, "—")),
                               "muted"));
        auto website = QUrl(user["website"].toString());
        if (website.scheme() == "https" || website.scheme() == "http")
            l->addWidget(Ui::button(website.toDisplayString() + " ↗",
                                    [website] { QDesktopServices::openUrl(website); }));
        auto actions = new QHBoxLayout;
        l->addLayout(actions);
        if (own) {
            actions->addWidget(Ui::button("Изменить профиль", [this] { editProfile(); }, c, "primary"));
        } else {
            auto uuid = Json::id(user);
            bool following = user["isFollowing"].toBool(user["is_following"].toBool());
            auto follow = new QPushButton(following ? "Отписаться" : "Подписаться");
            follow->setObjectName("primary");
            actions->addWidget(follow);
            connect(follow, &QPushButton::clicked, c, [this, client, uuid, following, follow] {
                follow->setEnabled(false);
                client->request(following ? "DELETE" : "POST", "/api/v1/app/users/" + seg(uuid) + "/follow",
                                {}, {}, follow, [this, follow](const ApiResult &r) {
                                    follow->setEnabled(true);
                                    if (!r.ok)
                                        Ui::error(this, r.error);
                                    else
                                        profile(m_viewedProfile);
                                });
            });
            actions->addWidget(Ui::button(
                "Написать",
                [this, user] {
                    auto a = m_accounts.active();
                    if (!a)
                        return;
                    auto d = dialog(this, "Сообщения", 960, 720);
                    auto layout = new QVBoxLayout(d);
                    auto msg = new MessagesPage(api(), &m_accounts, a->key, d);
                    layout->addWidget(msg);
                    msg->startDm(Json::id(user));
                    d->show();
                },
                c));
            bool blocked = user["isBlocked"].toBool(user["is_blocked"].toBool());
            actions->addWidget(Ui::button(
                blocked ? "Разблокировать" : "Заблокировать",
                [this, client, uuid, blocked, c] {
                    if (QMessageBox::question(this, "Блокировка",
                                              blocked ? "Разблокировать пользователя?"
                                                      : "Заблокировать пользователя?") != QMessageBox::Yes)
                        return;
                    client->request(blocked ? "DELETE" : "POST", "/api/v1/app/users/" + seg(uuid) + "/block",
                                    {}, {}, c, [this](const ApiResult &r) {
                                        if (!r.ok)
                                            Ui::error(this, r.error);
                                        else
                                            profile(m_viewedProfile);
                                    });
                },
                c));
        }
        m_content->addWidget(c);
        auto relations = new QHBoxLayout;
        m_content->addLayout(relations);
        for (const auto &entry : QList<QPair<QString, QString>>{{"Подписчики", "/api/followers/"},
                                                                {"Подписки", "/api/following/"},
                                                                {"Общие друзья", "/api/mutual-friends/"}}) {
            relations->addWidget(Ui::button(entry.first, [this, client, user, path = entry.second] {
                auto d = dialog(this, "Связи");
                auto outer = new QVBoxLayout(d);
                auto scroll = new QScrollArea;
                scroll->setWidgetResizable(true);
                auto body = new QWidget;
                auto list = new QVBoxLayout(body);
                list->setAlignment(Qt::AlignTop);
                scroll->setWidget(body);
                outer->addWidget(scroll);
                auto page = std::make_shared<int>(1);
                auto load = std::make_shared<std::function<void()>>();
                QPointer<QDialog> guard = d;
                *load = [this, client, user, path, list, page, guard] {
                    if (!guard)
                        return;
                    Ui::clear(list);
                    client->get(
                        path + seg(Json::text(user, {"id"})), guard,
                        [this, list, guard](const ApiResult &r) {
                            if (!guard)
                                return;
                            if (!r.ok) {
                                list->addWidget(Ui::label(r.error));
                                return;
                            }
                            renderList(r.data, list, guard, "users");
                        },
                        QUrlQuery{{"page", QString::number(*page)}, {"pageSize", "30"}});
                };
                auto next = Ui::button("Далее →", [page, load] {
                    ++*page;
                    (*load)();
                });
                outer->addWidget(next);
                (*load)();
                d->show();
            }));
        }
        auto posts = new QVBoxLayout;
        posts->setSpacing(18);
        m_content->addLayout(posts);
        auto postPage = std::make_shared<int>(1);
        auto more = Ui::button("Ещё публикации", [] {});
        m_content->addWidget(more);
        auto load = [this, client, user, posts, more, postPage] {
            more->setEnabled(false);
            client->get(
                "/api/users/" + seg(Json::text(user, {"id"})) + "/posts", more,
                [this, client, posts, more, postPage](const ApiResult &r) {
                    more->setEnabled(true);
                    if (!r.ok) {
                        failure(r);
                        return;
                    }
                    auto array = Json::posts(r.data);
                    for (auto v : array)
                        renderPost(v.toObject(), posts, client, m_page);
                    more->setVisible(!array.isEmpty());
                    if (array.isEmpty() && *postPage == 1)
                        posts->addWidget(Ui::label("Нет публикаций", "muted"));
                    ++*postPage;
                },
                QUrlQuery{{"page", QString::number(*postPage)}, {"pageSize", "20"}});
        };
        connect(more, &QPushButton::clicked, more, load);
        load();
    };
    if (id.isEmpty()) {
        render(u);
        return;
    }
    pageRequest("/api/profile/" + seg(id), {}, [this, render, own](const ApiResult &r) {
        auto user = Json::object(r.data, {"profile", "user", "data"});
        if (own && m_accounts.active())
            m_accounts.updateUser(m_accounts.activeKey(), user);
        render(user);
    });
}
void Window::stories(bool more) {
    if (!more) {
        m_items = new QVBoxLayout;
        m_items->setSpacing(16);
        m_content->addLayout(m_items);
        m_more = Ui::button("Ещё истории", [this] {
            ++m_number;
            stories(true);
        });
        m_content->addWidget(m_more);
    }
    m_more->setEnabled(false);
    auto client = api();
    client->get(
        "/api/v1/app/stories", m_page,
        [this](const ApiResult &r) {
            m_more->setEnabled(true);
            if (!r.ok) {
                failure(r);
                return;
            }
            auto groups = Json::list(r.data, {"users"});
            auto own = r.data.toObject()["current_user"];
            if (own.isObject() && m_number == 1)
                groups.prepend(own);
            int count = 0;
            for (auto g : groups) {
                auto user = g.toObject();
                if (user["user"].isObject()) {
                    auto nested = user["user"].toObject();
                    nested["stories"] = user["stories"];
                    user = nested;
                }
                auto stories = user["stories"].toArray();
                if (stories.isEmpty())
                    continue;
                QVBoxLayout *l;
                auto card = Ui::card(&l, m_page);
                l->addWidget(Ui::label("@" + user["username"].toString() +
                                           (user["has_unseen"].toBool() ? " · Новое" : ""),
                                       "sectionTitle"));
                for (auto s : stories) {
                    auto story = s.toObject();
                    auto expiry = QDateTime::fromString(story["expiration_time"].toString(), Qt::ISODate);
                    if (expiry.isValid() && expiry < QDateTime::currentDateTimeUtc())
                        continue;
                    l->addWidget(Ui::button(
                        Json::text(story, {"description"}, "Открыть историю") + " →",
                        [this, story, user] { showStory(story, user); }, card));
                    ++count;
                }
                m_items->addWidget(card);
            }
            m_more->setVisible(!groups.isEmpty());
            if (count == 0 && m_number == 1)
                m_items->addWidget(Ui::label("Пока нет активных историй", "postText"));
        },
        QUrlQuery{{"page", QString::number(m_number)}, {"limit", "30"}});
}
void Window::showStory(const QJsonObject &story, const QJsonObject &user) {
    auto d = dialog(this, "История");
    auto l = new QVBoxLayout(d);
    auto client = api();
    auto p = story;
    p["posted_by"] = user;
    renderPost(p, l, client, d, true);
    client->request("POST", "/api/v1/app/stories/" + seg(Json::id(story)) + "/view", {}, QJsonObject{}, d,
                    [d](const ApiResult &r) {
                        if (!r.ok)
                            Ui::error(d, r.error);
                    });
    l->addWidget(Ui::button("Зрители", [this, client, story, d, l] {
        client->get("/api/v1/app/stories/" + seg(Json::id(story)) + "/viewers", d,
                    [this, d, l](const ApiResult &r) {
                        if (!r.ok) {
                            Ui::error(d, r.error);
                            return;
                        }
                        renderList(r.data, l, d, "users");
                    });
    }));
    d->show();
}
void Window::notifications(bool more) {
    if (!more) {
        auto filter = new QComboBox;
        for (auto entry : QList<QPair<QString, QString>>{{"Все", "all"},
                                                         {"Реакции", "reactions"},
                                                         {"Комментарии", "comments"},
                                                         {"Упоминания", "mentions"},
                                                         {"Запросы", "requests"}})
            filter->addItem(entry.first, entry.second);
        filter->setCurrentIndex(qMax(0, filter->findData(m_notificationFilter)));
        m_toolbar->addWidget(filter);
        connect(filter, &QComboBox::currentIndexChanged, m_page, [this, filter] {
            m_notificationFilter = filter->currentData().toString();
            navigate(5);
        });
        m_toolbar->addWidget(Ui::button("Прочитать все", [this] {
            api()->request("POST", "/api/notification/mark-all-seen", {}, QJsonObject{}, m_page,
                           [this](const ApiResult &r) {
                               if (!r.ok)
                                   failure(r);
                               else
                                   navigate(5);
                           });
        }));
        m_items = new QVBoxLayout;
        m_content->addLayout(m_items);
        m_more = Ui::button("Ещё уведомления", [this] {
            ++m_number;
            notifications(true);
        });
        m_content->addWidget(m_more);
        api()->get("/api/notification-count", m_page, [this](const ApiResult &r) {
            if (r.ok)
                m_subtitle->setText("Непрочитанных: " + Json::text(r.data.toObject(), {"count"}, "0"));
        });
    }
    auto client = api();
    client->get(
        "/api/notification", m_page,
        [this, client](const ApiResult &r) {
            if (!r.ok) {
                failure(r);
                return;
            }
            auto arr = Json::list(r.data);
            for (auto v : arr) {
                auto o = v.toObject();
                QVBoxLayout *l;
                auto c = Ui::card(&l, m_page);
                auto who = Json::object(o.value("user"));
                if (who.isEmpty())
                    who = o["from_user"].toObject();
                l->addWidget(
                    Ui::label(Ui::userName(who) + " · " + Json::text(o, {"type", "notification_type"}),
                              "sectionTitle"));
                l->addWidget(
                    Ui::label(Json::text(o, {"message", "body", "text", "description"}), "postText"));
                l->addWidget(Ui::label(prettyTime(o), "muted"));
                auto actions = new QHBoxLayout;
                l->addLayout(actions);
                actions->addWidget(Ui::button("Прочитано", [this, client, o, c] {
                    client->request("POST", "/api/notification/seen/" + seg(Json::text(o, {"id"})), {},
                                    QJsonObject{}, c, [this, c](const ApiResult &r) {
                                        if (!r.ok)
                                            failure(r);
                                        else
                                            c->setEnabled(false);
                                    });
                }));
                auto post = Json::object(o.value("post"));
                if (!post.isEmpty())
                    actions->addWidget(Ui::button("Открыть публикацию", [this, client, post] {
                        client->get("/api/v1/app/post/" + seg(Json::id(post)), m_page,
                                    [this, client](const ApiResult &r) {
                                        if (!r.ok) {
                                            failure(r);
                                            return;
                                        }
                                        auto d = dialog(this, "Публикация");
                                        auto l = new QVBoxLayout(d);
                                        renderPost(Json::object(r.data, {"post"}), l, client, d);
                                        d->show();
                                    });
                    }));
                actions->addWidget(Ui::button("Подробнее", [this, o] { Ui::json(this, "Уведомление", o); }));
                m_items->addWidget(c);
            }
            m_more->setVisible(!arr.isEmpty());
            if (arr.isEmpty() && m_number == 1)
                m_items->addWidget(Ui::label("Новых уведомлений нет", "postText"));
        },
        QUrlQuery{{"page", QString::number(m_number)},
                  {"pagination_size", "15"},
                  {"filter", m_notificationFilter}});
}

void Window::comments(const QJsonObject &p) {
    auto d = new CommentsDialog(api(), p, this);
    d->show();
}
void Window::compose() {
    auto client = api();
    if (!client) {
        addAccount(false);
        return;
    }
    auto key = m_accounts.activeKey();
    client->get("/api/v1/app/entitlements/quotas", this, [this, client, key](const ApiResult &r) {
        if (m_accounts.activeKey() != key)
            return;
        if (!r.ok) {
            failure(r);
            return;
        }
        m_quotas = r.data.toObject();
        auto d = new ComposeDialog(client, m_quotas, this);
        connect(d, &QDialog::accepted, this, [this, key] {
            if (m_accounts.activeKey() == key)
                refresh();
        });
        d->show();
    });
}
void Window::editProfile() {
    auto d = new ProfileEditDialog(api(), &m_accounts, m_accounts.activeKey(), this);
    connect(d, &QDialog::accepted, this, [this] { navigate(6); });
    d->show();
}
void Window::people() {
    m_subtitle->setText("Подписки, запросы и ваши связи");
    auto types = new QComboBox;
    types->addItem("Запросы подписки", "/api/follow/requests");
    types->addItem("Заблокированные", "/api/v1/app/users/blocked");
    auto a = m_accounts.active();
    auto id = a ? Json::text(a->user, {"id"}) : QString();
    types->addItem("Подписчики", "/api/followers/" + seg(id));
    types->addItem("Подписки", "/api/following/" + seg(id));
    types->addItem("Близкие друзья", "/api/close-friends/" + seg(id));
    types->addItem("Скрытые истории", "/api/friends-hide-story");
    m_toolbar->addWidget(types, 1);
    m_items = new QVBoxLayout;
    m_content->addLayout(m_items);
    auto run = [this, types] {
        Ui::clear(m_items);
        auto path = types->currentData().toString();
        pageRequest(path, QUrlQuery{{"page", QString::number(m_number)}, {"pageSize", "30"}},
                    [this, path](const ApiResult &r) {
                        if (path == "/api/follow/requests") {
                            auto requests = Json::list(r.data, {"requests"});
                            if (requests.isEmpty())
                                m_items->addWidget(Ui::label("Запросов нет", "postText"));
                            for (auto v : requests) {
                                auto o = v.toObject();
                                QVBoxLayout *l;
                                auto card = Ui::card(&l, m_page);
                                auto u = Json::object(o.value("requester"));
                                if (u.isEmpty())
                                    u = Json::object(o.value("user"));
                                if (u.isEmpty())
                                    u = o["from_user"].toObject();
                                l->addWidget(Ui::label(Ui::userName(u), "sectionTitle"));
                                auto row = new QHBoxLayout;
                                l->addLayout(row);
                                for (auto e : QList<QPair<QString, QString>>{{"Принять", "ACCEPT"},
                                                                             {"Отклонить", "REJECT"}})
                                    row->addWidget(Ui::button(e.first, [this, card, o, action = e.second] {
                                        auto id = o.contains("id") ? o["id"] : o["request_id"];
                                        api()->request("POST", "/api/follow/manage-request", {},
                                                       QJsonObject{{"requestId", id}, {"action", action}},
                                                       card, [this, card](const ApiResult &r) {
                                                           if (!r.ok)
                                                               Ui::error(this, r.error);
                                                           else
                                                               card->hide();
                                                       });
                                    }));
                                m_items->addWidget(card);
                            }
                        } else
                            renderList(r.data, m_items, m_page, "users");
                    });
    };
    connect(types, &QComboBox::currentIndexChanged, m_page, [this, run] {
        m_number = 1;
        run();
    });
    m_toolbar->addWidget(Ui::button("Обновить", run));
    m_toolbar->addWidget(Ui::button("←", [this, run] {
        if (m_number > 1) {
            --m_number;
            run();
        }
    }));
    m_toolbar->addWidget(Ui::button("→", [this, run] {
        ++m_number;
        run();
    }));
    m_content->addWidget(Ui::button("Настроить близких друзей / скрытые истории",
                                    [this] { operation("PUT /api/add-close-friends"); }));
    run();
}
void Window::media() {
    m_subtitle->setText("Загруженные вами изображения, видео и файлы");
    auto client = api();
    m_toolbar->addWidget(Ui::button("Загрузить файл…", [this, client] {
        auto files = QFileDialog::getOpenFileNames(this, "Загрузить файлы");
        if (files.isEmpty())
            return;
        client->upload("/api/upload", {}, files, "files", m_page, [this](const ApiResult &r) {
            if (!r.ok)
                failure(r);
            else
                navigate(8);
        });
    }));
    pageRequest("/api/media/my-files", {}, [this, client](const ApiResult &r) {
        auto files = Json::list(r.data);
        if (files.isEmpty())
            m_content->addWidget(Ui::label("Здесь появятся ваши файлы", "postText"));
        for (auto v : files) {
            auto media = v.toObject();
            QVBoxLayout *l;
            auto card = Ui::card(&l, m_page);
            l->addWidget(
                Ui::label(Json::text(media, {"name", "filename", "publicId"}, "Медиафайл"), "sectionTitle"));
            auto url = Ui::mediaUrl(media);
            if (url.isRelative())
                url = client->baseUrl().resolved(url);
            if (media["mime"].toString().startsWith("image/"))
                l->addWidget(new RemoteImage(url, card, 260));
            auto actions = new QHBoxLayout;
            l->addLayout(actions);
            actions->addWidget(Ui::button("Открыть", [this, url, media] {
                if (media["mime"].toString().startsWith("video/") ||
                    media["mime"].toString().startsWith("audio/"))
                    play(this, url);
                else if (url.scheme() == "https" || url.scheme() == "http")
                    QDesktopServices::openUrl(url);
            }));
            actions->addWidget(Ui::button("Удалить", [this, client, media, card] {
                if (QMessageBox::question(this, "Удалить файл?", "Файл будет удалён с сервера.") !=
                    QMessageBox::Yes)
                    return;
                client->request("DELETE", "/api/media/" + seg(Json::text(media, {"id", "publicId"})), {}, {},
                                card, [this, card](const ApiResult &r) {
                                    if (!r.ok)
                                        failure(r);
                                    else
                                        card->hide();
                                });
            }));
            m_content->addWidget(card);
        }
    });
}
void Window::plans() {
    m_subtitle->setText("Текущий тариф, квоты и приглашения");
    auto client = api();
    for (auto e : QList<QPair<QString, QString>>{{"Моя подписка", "/api/v1/app/subscription"},
                                                 {"Мои квоты", "/api/v1/app/entitlements/quotas"},
                                                 {"Приглашения", "/api/v1/app/referrals/me"}}) {
        QVBoxLayout *l;
        auto card = Ui::card(&l, m_page);
        l->addWidget(Ui::label(e.first, "sectionTitle"));
        auto text = Ui::label("Загрузка…", "muted");
        text->setTextInteractionFlags(Qt::TextSelectableByMouse);
        l->addWidget(text);
        client->get(e.second, card, [this, text, l, card](const ApiResult &r) {
            if (!r.ok) {
                text->setText(r.error);
                return;
            }
            auto o = r.data.toObject();
            QStringList lines;
            for (auto it = o.begin(); it != o.end(); ++it) {
                if (it.value().isString() || it.value().isDouble() || it.value().isBool())
                    lines.append(it.key() + ": " +
                                 (it.value().isString() ? it.value().toString()
                                                        : QString::fromUtf8(Json::encode(it.value()))));
            }
            text->setText(lines.isEmpty() ? "Данные загружены" : lines.join('\n'));
            auto quotas = o["quotas"].toObject();
            for (auto it = quotas.begin(); it != quotas.end(); ++it) {
                auto q = it.value().toObject();
                l->addWidget(
                    Ui::label(it.key() + QString(" · %1 / %2 · осталось %3")
                                             .arg(Json::text(q, {"used"}, "—"), Json::text(q, {"limit"}, "—"),
                                                  Json::text(q, {"remaining"}, "—"))));
            }
            l->addWidget(
                Ui::button("Все данные", [this, r] { Ui::json(this, "Данные аккаунта", r.data); }, card));
        });
        m_content->addWidget(card);
    }
    m_content->addWidget(
        Ui::button("Сравнить тарифы", [this] { operation("GET /api/v1/app/entitlements/tiers"); }));
    m_content->addWidget(Ui::button("Синхронизировать подписку", [this, client] {
        client->request("POST", "/api/v1/app/subscription/refresh", {}, QJsonObject{}, m_page,
                        [this](const ApiResult &r) {
                            if (!r.ok)
                                failure(r);
                            else
                                navigate(9);
                        });
    }));
    m_content->addWidget(Ui::button("Кто пришёл по моей ссылке",
                                    [this] { operation("GET /api/v1/app/referrals/me/referees"); }));
}
void Window::settings() {
    m_subtitle->setText("Профиль, приватность и управление сессией");
    auto a = m_accounts.active();
    if (!a)
        return;
    QVBoxLayout *l;
    auto card = Ui::card(&l, m_page);
    l->addWidget(Ui::label("Ваш аккаунт", "sectionTitle"));
    l->addWidget(Ui::label(a->email + "\n" + a->baseUrl().toDisplayString(), "muted"));
    l->addWidget(Ui::button("Редактировать профиль", [this] { editProfile(); }, card, "primary"));
    m_content->addWidget(card);
    for (auto e :
         QList<QPair<QString, QString>>{{"Правила упоминаний", "PUT /api/v1/app/mention-policy"},
                                        {"Предпочтения обзора", "PUT /api/v1/app/user/discover-preferences"},
                                        {"Управление алгоритмом", "PUT /api/algorithm-control"},
                                        {"Проверить текущий пароль", "POST /api/v1/app/user/verify-password"},
                                        {"Сменить пароль", "POST /api/update-password"},
                                        {"Смена email: отправить код", "POST /api/update-email/send-otp"},
                                        {"Смена email: проверить код", "POST /api/update-email/verify-otp"},
                                        {"Смена email: завершить", "POST /api/reset-email"},
                                        {"Маркетинговые письма", "PUT /api/accept-marketing"},
                                        {"Отменить удаление аккаунта", "GET /api/delete-account-revoke"},
                                        {"Удалить аккаунт на сервере", "POST /api/delete-account"}})
        m_content->addWidget(
            Ui::button(e.first + " →", [this, match = e.second] { operation(match); }, m_page));
    auto client = api();
    auto key = a->key;
    m_content->addWidget(Ui::button("Выйти из аккаунта", [this, key, client] {
        if (QMessageBox::question(this, "Выйти?",
                                  "Сессия этого аккаунта будет завершена, локальные токены удалены.") !=
            QMessageBox::Yes)
            return;
        client->request("POST", "/api/logout", {}, QJsonObject{}, m_page, [this, key](const ApiResult &r) {
            if (!r.ok) {
                failure(r);
                return;
            }
            m_accounts.remove(key);
            if (auto c = m_clients.take(key))
                c->deleteLater();
        });
    }));
    m_content->addWidget(Ui::button("Убрать аккаунт с этого устройства", [this, key] {
        if (QMessageBox::question(this, "Убрать аккаунт?",
                                  "Локальные токены будут удалены. Аккаунт на сервере сохранится.") !=
            QMessageBox::Yes)
            return;
        m_accounts.remove(key);
        if (auto c = m_clients.take(key))
            c->deleteLater();
    }));
    if (!m_accounts.storageWarning().isEmpty())
        m_content->addWidget(Ui::label(m_accounts.storageWarning(), "muted"));
}
void Window::operation(const QString &match) {
    auto a = m_accounts.active();
    if (!a)
        return;
    auto d = dialog(this, "Метод API", 1120, 800);
    auto layout = new QVBoxLayout(d);
    layout->addWidget(new OperationPanel(api(), a->development, d, match));
    d->show();
}
void Window::showPreview() {
    newPage("Лента", "Предпросмотр интерфейса · демонстрационные данные");
    m_nav[0]->setChecked(true);
    m_environment->setText("ПРЕДПРОСМОТР");
    m_status->setText("Демонстрация оформления — сетевые запросы не выполняются.");
    auto client = new ApiClient(QUrl("https://api.monnettsocial.com"), this);
    QJsonObject user{{"id", 1}, {"publicId", "demo"}, {"name", "Команда Monnett"}, {"username", "monnett"}};
    QJsonObject post{{"id", 1},
                     {"publicId", "preview"},
                     {"posted_by", user},
                     {"description",
                      "Ваш мир начинается с людей.\n\nМоменты, которые хочется сохранить. Истории, "
                      "которыми хочется поделиться. И разговоры, которые продолжаются за пределами ленты."},
                     {"createdAt", QDateTime::currentDateTime().toString(Qt::ISODate)},
                     {"likes_count", 128},
                     {"comments_count", 16}};
    auto layout = new QVBoxLayout;
    m_content->addLayout(layout);
    renderPost(post, layout, client, m_page);
    QVBoxLayout *l;
    auto card = Ui::card(&l, m_page);
    l->addWidget(Ui::label("Всё своё. В одном месте.", "title"));
    l->addWidget(Ui::label("Несколько аккаунтов · Фото и видео · Шифрованные сообщения", "postText"));
    l->addWidget(Ui::label("Нативный клиент на C++ и Qt 6 для Linux", "muted"));
    m_content->addWidget(card);
    m_page->setEnabled(false);
}
