#pragma once
#include "api.h"
#include <QDialog>
class QVBoxLayout;class QLineEdit;class QComboBox;class QPlainTextEdit;class QPushButton;class QLabel;class QProgressBar;class QListWidget;class QCheckBox;
class CommentsDialog:public QDialog {
    Q_OBJECT
public:
    CommentsDialog(ApiClient*api,const QJsonObject&post,QWidget*parent=nullptr);
private:
    void load();void render(const QJsonObject&comment,QVBoxLayout*target,QWidget*context);
    ApiClient*m_api;QString m_post,m_parent;int m_page=1;QVBoxLayout*m_list;QPlainTextEdit*m_input;QLabel*m_status;QPushButton*m_send;QWidget*m_listWidget;
};
class ComposeDialog:public QDialog {
    Q_OBJECT
public:
    ComposeDialog(ApiClient*api,const QJsonObject&quotas,QWidget*parent=nullptr);
protected:
    void reject()override;
private:
    void publish();void uploadNext();void poll();void fail(const QString&message);
    ApiClient*m_api;QJsonObject m_quotas;QString m_postId;QStringList m_paths;int m_index=0,m_polls=0;bool m_busy=false,m_done=false;
    QPlainTextEdit*m_text,*m_extra;QLineEdit*m_link;QComboBox*m_type,*m_audience;QCheckBox*m_mature;QListWidget*m_files;QLabel*m_status;QPushButton*m_publish,*m_choose;QProgressBar*m_progress;QWidget*m_form;
};
class ProfileEditDialog:public QDialog {
    Q_OBJECT
public:ProfileEditDialog(ApiClient*api,Accounts*accounts,const QString&key,QWidget*parent=nullptr);
};
