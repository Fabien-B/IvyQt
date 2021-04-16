#include <QCoreApplication>
#include <QDebug>
#include "ivyqt.h"
#include <QTimer>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    //auto localhost = "127.255.255.255";


    auto ivyqt = new IvyQt("plop", "plop Ready");
    ivyqt->setFlushTimeout(200);
    ivyqt->start("192.168.1.255", 2010);
    ivyqt->bindMessage("Hello (.*)", [=](Peer* sender, QStringList params){
       qDebug() << "Hello with " << params[0] << " from " << sender->name();
    });

    ivyqt->bindMessage("^^Yo (.*)", [=](Peer* sender, QStringList params){
       qDebug() << "Yo with " << params[0] << " from " << sender->name();
    });

    QObject::connect(ivyqt, &IvyQt::peerReady, [=](Peer* peer){
        qDebug() << peer->name() << "connected";
    });

    QObject::connect(ivyqt, &IvyQt::peerDied, [=](QString name){
        qDebug() << name << "died";
    });

    QObject::connect(ivyqt, &IvyQt::directMessage, [=](Peer* peer, int ident, QString params){
        qDebug() << "direct message from " << peer->name() << " id:" << ident << " params:" << params;
    });

    QObject::connect(ivyqt, &IvyQt::stopped, [=](){
        qDebug() << "restart on different port";
        ivyqt->start("192.168.1.255", 2011);
    });

    QObject::connect(ivyqt, &IvyQt::quitRequest, [=](Peer* peer){
        auto bindings = peer->getBindings();
        qDebug() << bindings;
    });

    QStringList list = {"Hello you", "Yo toi", "12 abc"};
    auto timer = new QTimer();
    timer->setInterval(2000);
    QObject::connect(timer, &QTimer::timeout, [=]() {
        static int i = 0;
        ivyqt->send(list[i]);
        i = (i+1) % list.length();
    });
    timer->start();

    return a.exec();
}
