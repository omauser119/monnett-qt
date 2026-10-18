#pragma once
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QCache>
class AddAccountButton:public QPushButton {
    Q_OBJECT
public:
    explicit AddAccountButton(QWidget*p=nullptr);
signals:
    void addRequested(bool development);
protected:
    void mousePressEvent(QMouseEvent*event)override;
};
class RemoteImage:public QLabel {
    Q_OBJECT
public:
    explicit RemoteImage(const QUrl &url,QWidget*parent=nullptr,int height=300);
protected:
    void resizeEvent(QResizeEvent*event)override;
private:
    void render();QPixmap m_pixmap;int m_height;
};
namespace Ui {
QLabel *label(const QString &text,const QString &objectName={},QWidget*parent=nullptr);
QPushButton *button(const QString &text,const std::function<void()> &action,QWidget*parent=nullptr,const QString&name={});
QFrame *card(QVBoxLayout **layout,QWidget*parent=nullptr);
void clear(QLayout*layout);
QString userName(const QJsonObject&obj);
QUrl mediaUrl(const QJsonObject&obj);
void error(QWidget*parent,const QString&message);
void json(QWidget*parent,const QString&title,const QJsonValue&value);
}
