#include "operationpanel.h"
#include "json.h"
#include "widgets.h"
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSplitter>
#include <QTabWidget>
OperationPanel::OperationPanel(ApiClient *a, bool dev, QWidget *p, const QString &match)
    : QWidget(p), m_api(a) {
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto filter = new QLineEdit;
    filter->setPlaceholderText("Найти метод, путь или название…");
    layout->addWidget(filter);
    auto split = new QSplitter;
    layout->addWidget(split);
    m_list = new QListWidget;
    split->addWidget(m_list);
    auto right = new QWidget;
    auto l = new QVBoxLayout(right);
    l->setContentsMargins(18, 0, 0, 0);
    split->addWidget(right);
    split->setSizes({300, 650});
    m_description = Ui::label({}, "muted");
    m_description->setTextInteractionFlags(Qt::TextSelectableByMouse);
    l->addWidget(m_description);
    m_formWidget = new QWidget;
    m_form = new QFormLayout(m_formWidget);
    m_form->setContentsMargins(0, 0, 0, 0);
    auto formScroll = new QScrollArea;
    formScroll->setWidgetResizable(true);
    formScroll->setWidget(m_formWidget);
    formScroll->setMaximumHeight(230);
    l->addWidget(formScroll);
    auto tabs = new QTabWidget;
    m_body = new QPlainTextEdit;
    m_body->setPlaceholderText("JSON тела запроса. Удалите необязательные поля, которые не хотите менять.");
    m_schema = new QPlainTextEdit;
    m_schema->setReadOnly(true);
    m_result = new QPlainTextEdit;
    m_result->setReadOnly(true);
    tabs->addTab(m_body, "Тело запроса · JSON");
    tabs->addTab(m_result, "Ответ");
    tabs->addTab(m_schema, "Контракт");
    l->addWidget(tabs, 1);
    auto file = Ui::button("Прикрепить файлы…", [this] {
        m_files = QFileDialog::getOpenFileNames(this, "Файлы");
        m_status->setText(QString("Выбрано файлов: %1").arg(m_files.size()));
    });
    l->addWidget(file);
    m_run = Ui::button("Выполнить запрос", [this] { execute(); }, this, "primary");
    l->addWidget(m_run);
    m_status = Ui::label({}, "muted");
    l->addWidget(m_status);
    m_operations = m_catalogue.operations(dev);
    int selected = 0;
    for (int i = 0; i < m_operations.size(); ++i) {
        auto &o = m_operations[i];
        auto item = new QListWidgetItem(o.method + "  " + o.path + "\n" + o.summary, m_list);
        item->setToolTip(o.source == "official"
                             ? "Официальная OpenAPI"
                             : "Восстановлен из Android 1.5.0; сервер может не поддерживать");
        if (!match.isEmpty() && (o.method + " " + o.path == match || o.path == match))
            selected = i;
    }
    connect(filter, &QLineEdit::textChanged, this, [this](const QString &q) {
        for (int i = 0; i < m_list->count(); ++i)
            m_list->item(i)->setHidden(!m_list->item(i)->text().contains(q, Qt::CaseInsensitive));
    });
    connect(m_list, &QListWidget::currentRowChanged, this, &OperationPanel::select);
    connect(m_run, &QPushButton::clicked, tabs, [tabs] { tabs->setCurrentIndex(1); });
    m_list->setCurrentRow(selected);
}
void OperationPanel::select(int row) {
    if (row < 0 || row >= m_operations.size())
        return;
    m_current = m_operations[row];
    while (m_form->rowCount())
        m_form->removeRow(0);
    m_fields.clear();
    m_files.clear();
    m_fileField.clear();
    m_status->clear();
    m_result->clear();
    auto &o = m_current;
    m_description->setText(
        o.method + " " + o.path + "\n" + o.summary + "\n" +
        (o.source == "official" ? "Официальная спецификация" : "Реверс Android: legacy-контракт") + "\n" +
        o.spec["description"].toString());
    m_schema->setPlainText(QString::fromUtf8(Json::encode(o.spec, true)));
    for (auto v : o.spec["parameters"].toArray()) {
        auto f = v.toObject();
        auto where = f["in"].toString();
        if (where != "path" && where != "query")
            continue;
        auto schema = m_catalogue.resolve(f["schema"].toObject(), o.schemas);
        auto edit = new QLineEdit;
        auto required = f["required"].toBool() || where == "path";
        edit->setPlaceholderText(schema["type"].toString() +
                                 (required ? " · обязательно" : " · необязательно"));
        if (schema.contains("default"))
            edit->setText(
                Json::text({{"v", schema["default"]}}, {"v"}, schema["default"].toBool() ? "true" : ""));
        m_form->addRow(f["name"].toString() + " (" + where + ")" + (required ? " *" : ""), edit);
        m_fields.append({f["name"].toString(), where, schema, edit, required});
    }
    auto content = o.spec["requestBody"].toObject()["content"].toObject();
    auto schema = content["application/json"].toObject()["schema"].toObject();
    if (content.contains("multipart/form-data")) {
        schema =
            m_catalogue.resolve(content["multipart/form-data"].toObject()["schema"].toObject(), o.schemas);
        QJsonObject fields;
        auto props = schema["properties"].toObject();
        for (auto it = props.begin(); it != props.end(); ++it) {
            auto fs = m_catalogue.resolve(it.value().toObject(), o.schemas);
            if (fs["format"] == "binary" || fs["items"].toObject()["format"] == "binary")
                m_fileField = it.key();
            else
                fields[it.key()] = m_catalogue.example(fs, o.schemas);
        }
        m_body->setPlainText(QString::fromUtf8(Json::encode(fields, true)));
    } else
        m_body->setPlainText(
            schema.isEmpty() ? QString()
                             : QString::fromUtf8(Json::encode(m_catalogue.example(schema, o.schemas), true)));
    m_body->setEnabled(content.contains("application/json") || content.contains("multipart/form-data"));
}
void OperationPanel::execute() {
    QString path = m_current.path;
    QUrlQuery query;
    for (auto &f : m_fields) {
        auto text = f.edit->text().trimmed();
        if (text.isEmpty()) {
            if (f.required) {
                m_status->setText("Заполните поле " + f.name);
                return;
            }
            continue;
        }
        if (f.where == "path")
            path.replace("{" + f.name + "}", ApiClient::segment(text));
        else if (f.schema["type"] == "array") {
            QString error;
            auto arr = Json::parse(text.toUtf8(), &error);
            if (!arr.isArray()) {
                m_status->setText("Массив query передаётся как JSON: [\"id1\",\"id2\"]");
                return;
            }
            for (auto value : arr.toArray())
                query.addQueryItem(f.name, value.isString() ? value.toString()
                                                            : QString::fromUtf8(Json::encode(value)));
        } else
            query.addQueryItem(f.name, text);
    }
    if (path.contains('{')) {
        m_status->setText("Не все параметры пути заполнены");
        return;
    }
    QJsonValue body;
    if (m_body->isEnabled() && !m_body->toPlainText().trimmed().isEmpty()) {
        QString error;
        body = Json::parse(m_body->toPlainText().toUtf8(), &error);
        if (!error.isEmpty()) {
            m_status->setText("Ошибка JSON: " + error);
            return;
        }
    }
    if (m_current.method != "GET" && m_current.method != "HEAD") {
        if (QMessageBox::question(
                this, "Выполнить действие?",
                m_current.method + " " + path + "\nДействие будет выполнено для текущего аккаунта.",
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
            return;
    }
    auto done = [this](const ApiResult &r) {
        m_run->setEnabled(true);
        m_status->setText(QString("HTTP %1 · ").arg(r.status) + (r.ok ? "Готово" : r.error));
        m_result->setPlainText(QString::fromUtf8(Json::encode(Json::redact(r.raw), true)));
    };
    m_run->setEnabled(false);
    m_status->setText("Запрос…");
    if (!m_fileField.isEmpty()) {
        if (m_files.isEmpty()) {
            m_status->setText("Прикрепите файл");
            m_run->setEnabled(true);
            return;
        }
        m_api->upload(path, body.toObject(), m_files, m_fileField, this, done);
    } else
        m_api->request(m_current.method.toUtf8(), path, query, body, this, done);
}
