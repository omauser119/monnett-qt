#pragma once
#include "api.h"
#include <QDialog>
class QLineEdit;class QComboBox;class QDateEdit;class QLabel;class QCheckBox;class QPushButton;
class AuthDialog:public QDialog {
    Q_OBJECT
public:
    AuthDialog(Accounts*accounts,bool development,QWidget*parent=nullptr);
private:
    void submit();void modeChanged();void call(const QString&path,const QJsonObject&body,ApiCallback callback);
    Accounts*m_accounts;bool m_dev;ApiClient*m_api;
    QComboBox*m_mode;QLineEdit*m_email,*m_password,*m_name,*m_otp,*m_newPassword,*m_referral;QDateEdit*m_birth;QCheckBox*m_terms;QLabel*m_status;QPushButton*m_submit;
    QString m_resetToken;
};
