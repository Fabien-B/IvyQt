#include <QCoreApplication>
#include <QDebug>
#include "ivyqt.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);



    auto ivyqt = new IvyQt("plop");
    ivyqt->start("127.255.255.255", 2010);
    ivyqt->bindMessage("Hello (.*)", [=](QStringList params){
       qDebug() << "name is " << params[0];
    });

    qDebug() << "go to exec";
    return a.exec();
}
