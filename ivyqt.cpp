#include "ivyqt.h"
#include <QUdpSocket>

IvyQt::IvyQt(QString name, QObject *parent) :
    QObject(parent),
    name(name), udp_socket(nullptr), nextBindId(0)
{

}

void IvyQt::start(QString domain, int udp_port) {
    /// setUpChannels action
    // start TCP server
    server = new QTcpServer(this);
    bool res = server->listen();
    assert(res == true);
    connect(server, &QTcpServer::newConnection, this, &IvyQt::tcphandle);
    int tcp_port = server->serverPort();

    // start UDP socket
    udp_socket = new QUdpSocket(this);
    udp_socket->bind(udp_port, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    connect(udp_socket, &QUdpSocket::readyRead, this, &IvyQt::udphandle);

    /// Broadcast action
    QByteArray data;
    data += "3 ";
    data += QString::number(tcp_port);
    data += " ohohoh ";
    data += name;
    data += "\n";

    udp_socket->writeDatagram(data, QHostAddress(domain), udp_port);

}


int IvyQt::bindMessage(QString regex, std::function<void(QStringList)> callback) {
    nextBindId++;
    Binding b = {
        regex,
        callback,
        nextBindId,
    };
    bindings[nextBindId] = b;

    // publish regex to all peers
    for(auto peer: qAsConst(peers)) {
        peer->bind(nextBindId, regex);
    }

    return nextBindId;
}

void IvyQt::unBindMessage(int bindId) {
    bindings.remove(bindId);
    // advise all peers to remove this regex
    for(auto peer: qAsConst(peers)) {
        peer->unBind(bindId);
    }
}

void IvyQt::stop() {
    for(auto peer: qAsConst(peers)) {
        peer->sendBye();
    }
    //TODO rest of the stop process
}

void IvyQt::send(QString message) {
    for(auto peer: qAsConst(peers)) {
        peer->sendMessage(message);
    }
}


void IvyQt::tcphandle() {
    // accept action
    auto tcp_socket = server->nextPendingConnection();
    newPeer(tcp_socket);
}

void IvyQt::udphandle() {
    while (udp_socket->hasPendingDatagrams()) {
        qDebug() << "new datagram";
        QByteArray datagram;
        datagram.resize(udp_socket->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;

        udp_socket->readDatagram(datagram.data(), datagram.size(),
                                &sender, &senderPort);
        QString msg = QString(datagram.data()).trimmed();
        qDebug() << "UDP said " << msg;
        // "<protocol number> <port> <???> <app name>"
        auto params = msg.split(" ");
        assert(params[0] == "3");
        quint16 tcp_port = params[1].toUInt();
        QString appName = params[3];
        if(tcp_port == server->serverPort() && appName == name) {
            //that should be my own message, ignoring it.
            return;
        }

        auto socket = new QTcpSocket(this);

        socket->connectToHost(sender, tcp_port);
        connect(socket, &QTcpSocket::connected, this, [=]() {
            newPeer(socket);
        });

    }
}

void IvyQt::newPeer(QTcpSocket* socket) {
    auto peer = new Peer(socket, this);
    peers.append(peer);
    connect(peer, &Peer::message, this, &IvyQt::handleMessage);
    // a new peer connected to me
    // send a synchro message and initial subscriptions.
    connect(peer, &Peer::peerDied, this, &IvyQt::deletePeer);

    peer->sendId(server->serverPort(), name);
    for(auto &b: bindings) {
        peer->bind(b.bindId, b.regex);
    }
    peer->end_initial_bindings();
}

void IvyQt::handleMessage(QString id, QStringList params) {
    int bindId = id.toInt();
    if(bindings.contains(bindId)) {
        bindings[bindId].callback(params);
    }
}


void IvyQt::deletePeer(Peer* peer) {
    qDebug() << peer->name() << "quited";
    peers.removeAll(peer);
    peer->deleteLater();
}
