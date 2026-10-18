#pragma once
#include "api.h"
#include "catalogue.h"
#include <QWidget>
class QListWidget;
class QPlainTextEdit;
class QFormLayout;
class QVBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QTabWidget;
class OperationPanel : public QWidget {
    Q_OBJECT
  public:
    OperationPanel(ApiClient *api, bool development, QWidget *parent = nullptr, const QString &match = {});

  private:
    void select(int row);
    void execute();
    struct Field {
        QString name, where;
        QJsonObject schema;
        QLineEdit *edit;
        bool required = false;
    };
    ApiClient *m_api;
    Catalogue m_catalogue;
    QVector<Operation> m_operations;
    Operation m_current;
    QVector<Field> m_fields;
    QListWidget *m_list;
    QLabel *m_description, *m_status;
    QPlainTextEdit *m_body, *m_result, *m_schema;
    QFormLayout *m_form;
    QWidget *m_formWidget;
    QPushButton *m_run, *m_save;
    QByteArray m_responseBody;
    QStringList m_files;
    QString m_fileField;
};
