#include "window.h"
#include <QApplication>
#include <QFile>
#include <QCommandLineParser>
#include <QTimer>
int main(int argc,char**argv){QApplication app(argc,argv);QCoreApplication::setOrganizationName("MonnettQt");QCoreApplication::setApplicationName("monnett-qt");QCoreApplication::setApplicationVersion("0.1.0");QApplication::setDesktopFileName("social.monnett.MonnettQt");app.setStyle("Fusion");QFile theme(":/theme.qss");if(theme.open(QIODevice::ReadOnly))app.setStyleSheet(QString::fromUtf8(theme.readAll()));QCommandLineParser parser;parser.setApplicationDescription("Независимый клиент Monnett для Linux");parser.addHelpOption();parser.addVersionOption();parser.addOption({"preview","Показать демонстрационный интерфейс без подключения"});parser.addOption({"screenshot","Сохранить снимок окна и выйти","file"});parser.process(app);Window window;if(parser.isSet("preview"))window.showPreview();window.show();if(parser.isSet("screenshot"))QTimer::singleShot(1200,&window,[&]{bool ok=window.grab().save(parser.value("screenshot"));app.exit(ok?0:1);});return app.exec();}
