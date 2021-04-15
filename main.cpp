#include <QCoreApplication>
#include <QDebug>
#include "ivyqt.h"
#include <QTimer>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    //auto localhost = "127.255.255.255";


    auto ivyqt = new IvyQt("plop", "plop Ready");
    ivyqt->start("192.168.1.255", 2010);
    ivyqt->bindMessage("Hello (.*)", [=](QStringList params){
       qDebug() << "Hello with " << params[0];
    });

    ivyqt->bindMessage("^^Yo (.*)", [=](QStringList params){
       qDebug() << "Yo with " << params[0];
    });

    QObject::connect(ivyqt, &IvyQt::peerReady, [=](QString name){
        qDebug() << name << "connected";
    });

    QObject::connect(ivyqt, &IvyQt::peerDied, [=](QString name){
        qDebug() << name << "died";
    });

    QObject::connect(ivyqt, &IvyQt::stopped, [=](){
        qDebug() << "restart on different port";
        ivyqt->start("192.168.1.255", 2011);
    });

    auto timer = new QTimer();
    timer->setInterval(4000);
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, [=]() {
        qDebug() << "stopping IvyQt...";
        ivyqt->stop();
    });
    timer->start();

    return a.exec();
}
