#pragma once
#include <QJsonObject>
#include <QVector>
struct Operation {QString method,path,summary,source,environment;QJsonObject spec,schemas;};
class Catalogue {
public:
    Catalogue();
    QVector<Operation> operations(bool development)const;
    QJsonObject resolve(const QJsonObject &schema,const QJsonObject &schemas)const;
    QJsonValue example(const QJsonObject &schema,const QJsonObject &schemas,int depth=0)const;
private:
    QJsonObject m_root;
};
