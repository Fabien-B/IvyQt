#include <QApplication>
#include <QtWidgets>
#include <QDebug>
#include "ivyqt.h"
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    //auto localhost = "127.255.255.255";

    auto window = new QMainWindow();
    auto central_widget = new QWidget(window);
    window->setCentralWidget(central_widget);
    auto lay = new QHBoxLayout(central_widget);
    auto label = new QLabel("label", window);
    lay->addWidget(label);
    auto b = new QPushButton("del label", window);
    lay->addWidget(b);

    QObject::connect(b, &QPushButton::clicked, [=]() {
        lay->removeWidget(label);
        label->deleteLater();
    });

    auto ivyqt = new IvyQt("plop", "plop Ready");
    ivyqt->start("192.168.1.255", 2010);

    ivyqt->bindMessage("^Yo (.*)", [=](Peer* sender, QStringList params){
       qDebug() << "Yo with " << params[0] << " from " << sender->name();
    });

    ivyqt->bindMessage("Hello (.*)", label, [=](Peer* sender, QStringList params) {
       qDebug() << "Hello with " << params[0] << " from " << sender->name();
       label->setText(params[0]);
    });



//    QObject::connect(ivyqt, &IvyQt::peerReady, [=](Peer* peer){
//        qDebug() << peer->name() << "connected";
//    });

//    QObject::connect(ivyqt, &IvyQt::peerDied, [=](QString name){
//        qDebug() << name << "died";
//    });

//    QObject::connect(ivyqt, &IvyQt::directMessage, [=](Peer* peer, int ident, QString params){
//        qDebug() << "direct message from " << peer->name() << " id:" << ident << " params:" << params;
//    });

//    QObject::connect(ivyqt, &IvyQt::stopped, [=](){
//        qDebug() << "restart on different port";
//        ivyqt->start("192.168.1.255", 2011);
//    });

//    QObject::connect(ivyqt, &IvyQt::quitRequest, [=](Peer* peer){
//        auto bindings = peer->getBindings();
//        qDebug() << bindings;
//    });

//    QStringList list = {"Hello you", "Yo toi", "12 abc"};
//    auto timer = new QTimer();
//    timer->setInterval(2000);
//    QObject::connect(timer, &QTimer::timeout, [=]() {
//        static int i = 0;
//        ivyqt->send(list[i]);
//        i = (i+1) % list.length();
//    });
//    timer->start();

    window->show();
    return a.exec();
}
