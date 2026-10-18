#pragma once
#include "api.h"
#include <QWidget>
#include <QPointer>
namespace Quotient{class Connection;class Room;}
class QListWidget;class QPlainTextEdit;class QLabel;class QPushButton;
class MessagesPage:public QWidget {
    Q_OBJECT
public:
    MessagesPage(ApiClient*api,Accounts*accounts,const QString&key,QWidget*parent=nullptr);
    ~MessagesPage()override;
    void startDm(const QString&userPublicId);
private:
    void start();void rooms();void selectRoom();void timeline();void send();void attach();
    ApiClient*m_api;Accounts*m_accounts;QString m_key,m_pendingDm;QPointer<Quotient::Connection>m_connection;QPointer<Quotient::Room>m_room;
    QListWidget*m_rooms,*m_events;QPlainTextEdit*m_input;QLabel*m_status;QPushButton*m_send,*m_attach;
};
