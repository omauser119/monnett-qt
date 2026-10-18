#include "widgets.h"
#include "json.h"
#include <QMouseEvent>
#include <QNetworkReply>
#include <QBuffer>
#include <QImageReader>
#include <QMessageBox>
#include <QDialog>
#include <QPlainTextEdit>
#include <QDialogButtonBox>
AddAccountButton::AddAccountButton(QWidget*p):QPushButton("＋  Добавить аккаунт",p){setObjectName("addAccount");setToolTip("Добавить аккаунт Monnett");connect(this,&QPushButton::clicked,this,[this]{emit addRequested(false);});setContextMenuPolicy(Qt::PreventContextMenu);}
void AddAccountButton::mousePressEvent(QMouseEvent*e){if(e->button()==Qt::RightButton&&e->modifiers().testFlag(Qt::ShiftModifier)&&e->modifiers().testFlag(Qt::AltModifier)){emit addRequested(true);e->accept();return;}QPushButton::mousePressEvent(e);}
RemoteImage::RemoteImage(const QUrl&u,QWidget*p,int h):QLabel(p),m_height(h){setAlignment(Qt::AlignCenter);setMinimumHeight(80);setMaximumHeight(h);setText("Загрузка изображения…");setObjectName("remoteImage");setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);if(u.scheme()!="https"&&u.scheme()!="http"){setText("Нет изображения");return;}auto manager=new QNetworkAccessManager(this);QNetworkRequest req(u);req.setTransferTimeout(20000);req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::NoLessSafeRedirectPolicy);auto reply=manager->get(req);connect(reply,&QNetworkReply::downloadProgress,this,[reply](qint64 got,qint64){if(got>24*1024*1024)reply->abort();});connect(reply,&QNetworkReply::finished,this,[this,reply]{auto bytes=reply->readAll();QBuffer buffer(&bytes);buffer.open(QIODevice::ReadOnly);QImageReader reader(&buffer);auto size=reader.size();if(size.width()>0&&size.height()>0&&qint64(size.width())*size.height()<=80000000){size.scale(1600,1200,Qt::KeepAspectRatio);reader.setScaledSize(size);m_pixmap=QPixmap::fromImage(reader.read());}reply->deleteLater();if(m_pixmap.isNull())setText("Изображение недоступно");else render();});}
void RemoteImage::resizeEvent(QResizeEvent*e){QLabel::resizeEvent(e);render();}
void RemoteImage::render(){if(!m_pixmap.isNull()){setPixmap(m_pixmap.scaled(qMax(width(),80),m_height,Qt::KeepAspectRatio,Qt::SmoothTransformation));setMinimumHeight(qMin(m_height,pixmap().height()));}}
QLabel*Ui::label(const QString&t,const QString&name,QWidget*p){auto w=new QLabel(t,p);w->setTextFormat(Qt::PlainText);w->setWordWrap(true);if(!name.isEmpty())w->setObjectName(name);return w;}
QPushButton*Ui::button(const QString&t,const std::function<void()>&action,QWidget*p,const QString&name){auto b=new QPushButton(t,p);if(!name.isEmpty())b->setObjectName(name);QObject::connect(b,&QPushButton::clicked,b,action);return b;}
QFrame*Ui::card(QVBoxLayout**l,QWidget*p){auto f=new QFrame(p);f->setObjectName("card");*l=new QVBoxLayout(f);(*l)->setContentsMargins(22,20,22,20);(*l)->setSpacing(13);return f;}
void Ui::clear(QLayout*l){while(auto item=l->takeAt(0)){if(item->widget()){item->widget()->hide();item->widget()->deleteLater();}if(item->layout())clear(item->layout());delete item;}}
QString Ui::userName(const QJsonObject&o){return Json::text(o,{"name","displayName","display_name","username"},"Monnett");}
QUrl Ui::mediaUrl(const QJsonObject&o){auto s=Json::text(o,{"url","media_url","mediaUrl","originalUrl","original_url","thumbnailUrl","thumbnail_url"});if(s.isEmpty()){auto f=o["formats"].toObject();s=f["medium"].toObject()["url"].toString();if(s.isEmpty())s=f["thumbnail"].toObject()["url"].toString();}return QUrl(s);}
void Ui::error(QWidget*p,const QString&m){QMessageBox::warning(p,"Monnett",m);}
void Ui::json(QWidget*p,const QString&t,const QJsonValue&v){auto d=new QDialog(p);d->setAttribute(Qt::WA_DeleteOnClose);d->setWindowTitle(t);d->resize(760,620);auto l=new QVBoxLayout(d);auto edit=new QPlainTextEdit(QString::fromUtf8(Json::encode(Json::redact(v),true)));edit->setReadOnly(true);l->addWidget(edit);auto buttons=new QDialogButtonBox(QDialogButtonBox::Close);QObject::connect(buttons,&QDialogButtonBox::rejected,d,&QDialog::reject);l->addWidget(buttons);d->show();}
