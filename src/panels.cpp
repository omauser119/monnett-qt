#include "panels.h"
#include "json.h"
#include "widgets.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
namespace {
QString s(const QString &id) {
    return ApiClient::segment(id);
}
} // namespace
CommentsDialog::CommentsDialog(ApiClient *a, const QJsonObject &p, QWidget *parent)
    : QDialog(parent), m_api(a), m_post(Json::id(p)) {
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle("Комментарии");
    resize(730, 780);
    auto l = new QVBoxLayout(this);
    l->addWidget(Ui::label("Обсуждение", "title"));
    auto scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    m_listWidget = new QWidget;
    m_list = new QVBoxLayout(m_listWidget);
    m_list->setAlignment(Qt::AlignTop);
    scroll->setWidget(m_listWidget);
    l->addWidget(scroll, 1);
    auto pages = new QHBoxLayout;
    pages->addWidget(Ui::button("←", [this] {
        if (m_page > 1) {
            --m_page;
            load();
        }
    }));
    pages->addWidget(Ui::button("Далее →", [this] {
        ++m_page;
        load();
    }));
    l->addLayout(pages);
    m_status = Ui::label({}, "muted");
    l->addWidget(m_status);
    m_input = new QPlainTextEdit;
    m_input->setPlaceholderText("Напишите комментарий…");
    m_input->setMaximumHeight(100);
    l->addWidget(m_input);
    auto actions = new QHBoxLayout;
    actions->addWidget(Ui::button("Отменить ответ", [this] {
        m_parent.clear();
        m_status->clear();
    }));
    m_send = Ui::button(
        "Отправить",
        [this] {
            auto text = m_input->toPlainText().trimmed();
            if (text.isEmpty())
                return;
            QJsonObject body{{"post_id", m_post}, {"comment", text}, {"mentions", QJsonArray{}}};
            if (!m_parent.isEmpty())
                body["parent_comment_id"] = m_parent;
            m_send->setEnabled(false);
            m_api->request("POST", "/api/v1/app/comments", {}, body, this, [this](const ApiResult &r) {
                m_send->setEnabled(true);
                if (!r.ok) {
                    m_status->setText(r.error);
                    return;
                }
                m_input->clear();
                m_parent.clear();
                m_page = 1;
                load();
            });
        },
        this, "primary");
    actions->addWidget(m_send);
    l->addLayout(actions);
    load();
}
void CommentsDialog::load() {
    Ui::clear(m_list);
    m_status->setText("Загрузка…");
    m_api->get(
        "/api/v1/app/posts/" + s(m_post) + "/comments", this,
        [this](const ApiResult &r) {
            if (!r.ok) {
                m_status->setText(r.error);
                return;
            }
            m_status->setText(QString("Страница %1").arg(m_page));
            auto list = Json::list(r.data, {"comments"});
            for (auto v : list)
                render(v.toObject(), m_list, m_listWidget);
            if (list.isEmpty())
                m_list->addWidget(Ui::label("Комментариев нет", "muted"));
        },
        QUrlQuery{{"page", QString::number(m_page)}, {"limit", "20"}});
}
void CommentsDialog::render(const QJsonObject &o, QVBoxLayout *target, QWidget *context) {
    QVBoxLayout *l;
    auto card = Ui::card(&l, context);
    auto author = Json::object(o.value("commented_by"));
    l->addWidget(
        Ui::label(Ui::userName(author) + (o["pinned"].toBool() ? " · Закреплено" : ""), "sectionTitle"));
    l->addWidget(Ui::label(o["comment"].toString(), "postText"));
    auto row = new QHBoxLayout;
    l->addLayout(row);
    auto id = Json::id(o);
    auto like = new QPushButton((o["is_liked"].toBool() ? "♥ " : "♡ ") +
                                Json::text(o, {"likes_count", "like_count"}, "0"));
    like->setCheckable(true);
    like->setChecked(o["is_liked"].toBool());
    row->addWidget(like);
    connect(like, &QPushButton::clicked, card, [this, id, like](bool on) {
        like->setEnabled(false);
        m_api->request(on ? "POST" : "DELETE", "/api/v1/app/comments/" + s(id) + "/like", {}, {}, like,
                       [this, like, on](const ApiResult &r) {
                           like->setEnabled(true);
                           if (!r.ok) {
                               like->setChecked(!on);
                               m_status->setText(r.error);
                               return;
                           }
                           auto o = r.data.toObject();
                           like->setText((on ? "♥ " : "♡ ") +
                                         Json::text(o, {"likes_count", "likeCount", "like_count"}, "Готово"));
                       });
    });
    row->addWidget(Ui::button("Ответить", [this, id, author] {
        m_parent = id;
        m_status->setText("Ответ для " + Ui::userName(author));
        m_input->setFocus();
    }));
    auto replies = new QVBoxLayout;
    l->addLayout(replies);
    auto replyPage = std::make_shared<int>(1);
    row->addWidget(
        Ui::button("Ответы · " + Json::text(o, {"replies_count"}, "0"), [this, id, replies, card, replyPage] {
            m_api->get(
                "/api/v1/app/comments/" + s(id) + "/replies", card,
                [this, replies, card, replyPage](const ApiResult &r) {
                    if (!r.ok) {
                        m_status->setText(r.error);
                        return;
                    }
                    for (auto v : Json::list(r.data, {"replies"}))
                        render(v.toObject(), replies, card);
                    ++*replyPage;
                },
                QUrlQuery{{"page", QString::number(*replyPage)}, {"limit", "20"}});
        }));
    auto more = new QPushButton("•••");
    row->addWidget(more);
    connect(more, &QPushButton::clicked, card, [this, id, o, card] {
        bool ok;
        auto action = QInputDialog::getItem(
            this, "Комментарий", "Действие",
            {o["pinned"].toBool() ? "Открепить" : "Закрепить", "Удалить", "Пожаловаться"}, 0, false, &ok);
        if (!ok)
            return;
        if (action == "Пожаловаться") {
            bool accepted;
            auto reason = QInputDialog::getText(this, "Жалоба", "Причина", QLineEdit::Normal, {}, &accepted);
            if (!accepted || reason.trimmed().isEmpty())
                return;
            m_api->request(
                "POST", "/api/reports", {}, QJsonObject{{"comment_public_id", id}, {"reason", reason}}, card,
                [this](const ApiResult &r) { m_status->setText(r.ok ? "Жалоба отправлена" : r.error); });
            return;
        }
        if (action == "Удалить" &&
            QMessageBox::question(this, "Удалить комментарий?", "Удалить этот комментарий на сервере?") !=
                QMessageBox::Yes)
            return;
        bool pin = action == "Закрепить";
        m_api->request(pin ? "POST" : "DELETE",
                       "/api/v1/app/comments/" + s(id) + (action == "Удалить" ? "" : "/pin"), {}, {}, card,
                       [this](const ApiResult &r) {
                           if (!r.ok)
                               m_status->setText(r.error);
                           else
                               load();
                       });
    });
    target->addWidget(card);
}
ComposeDialog::ComposeDialog(ApiClient *a, const QJsonObject &q, QWidget *p)
    : QDialog(p), m_api(a), m_quotas(q) {
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle("Новая публикация");
    resize(690, 780);
    auto l = new QVBoxLayout(this);
    l->addWidget(Ui::label("Поделитесь моментом", "title"));
    m_form = new QWidget;
    auto f = new QFormLayout(m_form);
    m_text = new QPlainTextEdit;
    m_text->setPlaceholderText("О чём вы думаете?");
    m_text->setMinimumHeight(150);
    m_link = new QLineEdit;
    m_link->setPlaceholderText("https://…");
    m_type = new QComboBox;
    m_type->addItem("Текст", "text");
    m_type->addItem("Фото / видео", "media");
    m_type->addItem("История", "story");
    m_audience = new QComboBox;
    m_audience->addItems({"PUBLIC", "FOLLOWERS", "CLOSE_FRIENDS"});
    m_mature = new QCheckBox("Материалы для взрослых");
    m_extra = new QPlainTextEdit("{}");
    m_extra->setMaximumHeight(95);
    m_extra->setToolTip("category_id, location, tagged_users, mentions и другие поля InitializePostRequest");
    f->addRow("Формат", m_type);
    f->addRow("Текст", m_text);
    f->addRow("Ссылка", m_link);
    f->addRow("Аудитория", m_audience);
    f->addRow(m_mature);
    f->addRow("Дополнительные поля JSON", m_extra);
    l->addWidget(m_form);
    m_files = new QListWidget;
    m_files->setMaximumHeight(110);
    l->addWidget(m_files);
    auto filesRow = new QHBoxLayout;
    m_choose = Ui::button("＋ Файлы", [this] {
        auto files = QFileDialog::getOpenFileNames(this, "Фото и видео", {},
                                                   "Media (*.jpg *.jpeg *.png *.webp *.gif *.avif *.mp4 "
                                                   "*.mov *.webm *.mkv *.mp3 *.ogg);;Все файлы (*)");
        for (auto &file : files)
            if (!m_paths.contains(file)) {
                m_paths.append(file);
                m_files->addItem(QFileInfo(file).fileName());
            }
        if (!m_paths.isEmpty() && m_type->currentData() == "text")
            m_type->setCurrentIndex(1);
    });
    filesRow->addWidget(m_choose);
    filesRow->addWidget(Ui::button("Убрать выбранный", [this] {
        if (m_busy || !m_postId.isEmpty())
            return;
        int i = m_files->currentRow();
        if (i >= 0) {
            m_paths.removeAt(i);
            delete m_files->takeItem(i);
        }
    }));
    l->addLayout(filesRow);
    m_progress = new QProgressBar;
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    l->addWidget(m_progress);
    m_status = Ui::label("Файлы загружаются последовательно. При ошибке можно продолжить с места остановки.",
                         "muted");
    l->addWidget(m_status);
    m_publish = Ui::button("Опубликовать", [this] { publish(); }, this, "primary");
    l->addWidget(m_publish);
    connect(m_api, &ApiClient::uploadProgress, this, [this](qint64 sent, qint64 total) {
        if (m_busy && total > 0)
            m_progress->setValue(int((m_index + double(sent) / total) * 100 / qMax(1, m_paths.size())));
    });
}
void ComposeDialog::reject() {
    if (m_busy) {
        Ui::error(this, "Дождитесь завершения текущего запроса.");
        return;
    }
    if (!m_postId.isEmpty() && !m_done &&
        QMessageBox::question(
            this, "Загрузка не завершена", "Оставить незавершённую публикацию на сервере? Её ID: " + m_postId,
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
        return;
    QDialog::reject();
}
void ComposeDialog::fail(const QString &e) {
    m_busy = false;
    m_publish->setEnabled(true);
    m_publish->setText(m_postId.isEmpty() ? "Повторить публикацию" : "Продолжить загрузку");
    m_status->setText(e + (m_postId.isEmpty() ? QString() : "\nID публикации: " + m_postId));
    if (m_postId.isEmpty()) {
        m_form->setEnabled(true);
        m_choose->setEnabled(true);
    }
}
void ComposeDialog::publish() {
    if (m_done) {
        accept();
        return;
    }
    if (m_busy)
        return;
    if (!m_postId.isEmpty()) {
        m_busy = true;
        m_publish->setEnabled(false);
        uploadNext();
        return;
    }
    auto text = m_text->toPlainText().trimmed();
    auto type = m_type->currentData().toString();
    if (text.isEmpty() && m_paths.isEmpty()) {
        m_status->setText("Добавьте текст или файл");
        return;
    }
    if (type != "text" && m_paths.isEmpty()) {
        m_status->setText("Для этого формата нужен хотя бы один файл");
        return;
    }
    auto quotas = m_quotas["quotas"].toObject();
    auto maxFiles = quotas["MAX_MEDIA_PER_POST"].toObject()["limit"].toInt(-1);
    auto maxSize = quotas["MAX_MEDIA_FILE_SIZE_MB"].toObject()["limit"].toDouble(-1);
    if (maxFiles >= 0 && m_paths.size() > maxFiles) {
        m_status->setText("Превышен лимит файлов вашего тарифа");
        return;
    }
    for (auto &file : m_paths) {
        QFileInfo info(file);
        if (!info.isReadable() || (maxSize >= 0 && info.size() > maxSize * 1024 * 1024)) {
            m_status->setText("Файл недоступен или превышает лимит: " + info.fileName());
            return;
        }
    }
    QString error;
    auto extra = Json::parse(m_extra->toPlainText().toUtf8(), &error);
    if (!extra.isObject() || !error.isEmpty()) {
        m_status->setText("Дополнительные поля должны быть объектом JSON");
        return;
    }
    QJsonObject body = extra.toObject();
    body["description"] = text;
    body["post_type"] = type;
    body["media_count"] = m_paths.size();
    body["share_with"] = m_audience->currentText();
    body["is_mature"] = m_mature->isChecked();
    if (!body.contains("mentions"))
        body["mentions"] = QJsonArray{};
    if (!m_link->text().trimmed().isEmpty())
        body["link"] = m_link->text().trimmed();
    m_busy = true;
    m_publish->setEnabled(false);
    m_form->setEnabled(false);
    m_choose->setEnabled(false);
    m_status->setText("Создание публикации…");
    m_api->request("POST", "/api/v1/app/post/create", {}, body, this, [this](const ApiResult &r) {
        if (!r.ok) {
            fail(r.error + "\nПри сетевой ошибке сначала проверьте профиль, чтобы избежать дубликата.");
            return;
        }
        m_postId = r.data.isString() ? r.data.toString()
                                     : Json::text(r.data.toObject(), {"postId", "post_id", "publicId", "id"});
        if (m_postId.isEmpty()) {
            fail("В ответе нет ID публикации. Проверьте профиль перед повтором.");
            return;
        }
        uploadNext();
    });
}
void ComposeDialog::uploadNext() {
    if (m_index >= m_paths.size()) {
        m_progress->setValue(100);
        m_polls = 0;
        poll();
        return;
    }
    m_status->setText(QString("Загрузка %1 из %2…").arg(m_index + 1).arg(m_paths.size()));
    m_api->upload("/api/v1/app/media/upload", {{"postId", m_postId}, {"mediaIndex", m_index}},
                  {m_paths[m_index]}, "file", this, [this](const ApiResult &r) {
                      if (!r.ok) {
                          fail(r.error);
                          return;
                      }
                      ++m_index;
                      uploadNext();
                  });
}
void ComposeDialog::poll() {
    m_status->setText("Файлы приняты. Проверка обработки публикации…");
    m_api->get(
        "/api/v1/app/post/status", this,
        [this](const ApiResult &r) {
            auto items = Json::list(r.data, {"statuses", "posts"});
            if (items.isEmpty() && r.data.toObject().contains("status"))
                items.append(r.data);
            QString status;
            if (!items.isEmpty())
                status = Json::text(items.first().toObject(), {"status", "post_status"}).toLower();
            if (r.ok && (status == "approved" || status == "published")) {
                m_busy = false;
                m_done = true;
                m_status->setText("Публикация готова · " + m_postId);
                m_publish->setText("Готово");
                m_publish->setEnabled(true);
                return;
            }
            if (r.ok && (status == "error" || status == "reported")) {
                m_busy = false;
                m_done = true;
                m_status->setText("Сервер завершил обработку со статусом: " + status + " · " + m_postId);
                m_publish->setText("Закрыть");
                m_publish->setEnabled(true);
                return;
            }
            if (!r.ok || ++m_polls >= 15) {
                m_busy = false;
                m_done = true;
                m_status->setText("Публикация отправлена, обработка продолжается на сервере. ID: " +
                                  m_postId + (r.ok ? QString() : "\nПроверка статуса: " + r.error));
                m_publish->setText("Готово");
                m_publish->setEnabled(true);
                return;
            }
            QTimer::singleShot(2000, this, [this] { poll(); });
        },
        QUrlQuery{{"ids", m_postId}});
}
ProfileEditDialog::ProfileEditDialog(ApiClient *api, Accounts *accounts, const QString &key, QWidget *p)
    : QDialog(p) {
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle("Редактировать профиль");
    resize(550, 650);
    auto a = accounts->find(key);
    if (!a)
        return;
    auto user = a->user;
    auto l = new QVBoxLayout(this);
    auto f = new QFormLayout;
    l->addLayout(f);
    auto name = new QLineEdit(user["name"].toString());
    auto username = new QLineEdit(user["username"].toString());
    auto bio = new QPlainTextEdit(user["bio"].toString());
    bio->setMaximumHeight(130);
    auto web = new QLineEdit(user["website"].toString());
    auto isPublic = new QCheckBox("Открытый профиль");
    isPublic->setChecked(user["isPublic"].toBool(user["is_public"].toBool()));
    auto restrict = new QCheckBox("Ограничить сообщения");
    restrict->setChecked(user["restrictMessages"].toBool(user["restrict_messages"].toBool()));
    f->addRow("Имя", name);
    f->addRow("Username", username);
    f->addRow("О себе", bio);
    f->addRow("Сайт", web);
    f->addRow(isPublic);
    f->addRow(restrict);
    auto status = Ui::label({}, "muted");
    l->addWidget(status);
    l->addWidget(Ui::button("Изменить аватар…", [this, api, status] {
        auto path = QFileDialog::getOpenFileName(this, "Аватар", {}, "Images (*.png *.jpg *.jpeg *.webp)");
        if (path.isEmpty())
            return;
        api->upload("/api/upload", {}, {path}, "files", this, [this, api, status](const ApiResult &r) {
            if (!r.ok) {
                status->setText(r.error);
                return;
            }
            auto arr = Json::list(r.data);
            if (arr.isEmpty()) {
                status->setText("Сервер не вернул ID файла");
                return;
            }
            auto media = arr.first().toObject();
            auto id = media.contains("publicId") ? media["publicId"] : media["id"];
            api->request(
                "PUT", "/api/profile", {}, QJsonObject{{"profile_picture_id", id}}, this,
                [status](const ApiResult &r) { status->setText(r.ok ? "Аватар обновлён" : r.error); });
        });
    }));
    auto save = new QPushButton("Сохранить");
    save->setObjectName("primary");
    l->addWidget(save);
    connect(save, &QPushButton::clicked, this, [=, this] {
        save->setEnabled(false);
        status->setText("Сохранение…");
        auto finish = [=, this] {
            api->get("/api/user", this, [=, this](const ApiResult &r) {
                save->setEnabled(true);
                if (!r.ok) {
                    status->setText(r.error);
                    return;
                }
                accounts->updateUser(key, Json::object(r.data, {"user"}));
                accept();
            });
        };
        auto saveBio = [=, this] {
            api->request("PUT", "/api/v1/app/profile/bio", {},
                         QJsonObject{{"bio", bio->toPlainText()}, {"mentions", QJsonArray{}}}, this,
                         [=](const ApiResult &r) {
                             if (!r.ok) {
                                 save->setEnabled(true);
                                 status->setText(r.error);
                                 return;
                             }
                             finish();
                         });
        };
        auto saveProfile = [=, this] {
            api->request("PUT", "/api/profile", {},
                         QJsonObject{{"name", name->text()},
                                     {"website", web->text()},
                                     {"is_public", isPublic->isChecked()},
                                     {"restrict_messages", restrict->isChecked()}},
                         this, [=](const ApiResult &r) {
                             if (!r.ok) {
                                 save->setEnabled(true);
                                 status->setText(r.error);
                                 return;
                             }
                             saveBio();
                         });
        };
        if (username->text() != user["username"].toString()) {
            api->get(
                "/api/check-username", this,
                [=, this](const ApiResult &r) {
                    if (!r.ok) {
                        save->setEnabled(true);
                        status->setText(r.error);
                        return;
                    }
                    api->request("PUT", "/api/username", {},
                                 QJsonObject{{"username", username->text().trimmed()}}, this,
                                 [=](const ApiResult &r) {
                                     if (!r.ok) {
                                         save->setEnabled(true);
                                         status->setText(r.error);
                                         return;
                                     }
                                     saveProfile();
                                 });
                },
                QUrlQuery{{"username", username->text().trimmed()}});
        } else
            saveProfile();
    });
}
