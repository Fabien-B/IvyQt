#include "ivyqt.h"
#include <QUdpSocket>
#include <QRandomGenerator>

IvyQt::IvyQt(QString name, QString msgReady, QObject *parent) :
    QObject(parent),
    name(name), msgReady(msgReady),
    udp_socket(nullptr), server(nullptr),
    nextBindId(0), flush_timeout(0),
    running(false), stopRequested(false)
{

}

void IvyQt::start(QString domain, int udp_port) {
    stopRequested = false;
    // start TCP server
    server = new QTcpServer(this);
    bool res = server->listen();
    assert(res == true);
    connect(server, &QTcpServer::newConnection, this, &IvyQt::tcphandle);

    // start UDP socket
    udp_socket = new QUdpSocket(this);
    udp_socket->bind(udp_port, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    connect(udp_socket, &QUdpSocket::readyRead, this, &IvyQt::udphandle);

    // generate a uniq ID
    watcherId = QString::number(QRandomGenerator::system()->generate64());

    // forge broadcast data
    QByteArray data;
    data += QString("3 %1 %2 %3\n").arg(QString::number(server->serverPort()), watcherId, name);
    udp_socket->writeDatagram(data, QHostAddress(domain), udp_port);

    running = true;
}


int IvyQt::bindMessage(QString regex, std::function<void(Peer*, QStringList)> callback) {
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
    stopRequested = true;
    if(!running) {
        return;
    }
    if(peers.length() == 0) {
        completeStop();
    } else {
        //stop will completed after all peer has been deleted
        for(auto peer: qAsConst(peers)) {
            peer->sendBye();
            peer->stop();
        }
    }

}

void IvyQt::send(QString message) {
    for(auto peer: qAsConst(peers)) {
        peer->sendMessage(message);
    }
}

QStringList IvyQt::getPeers() {
    QStringList peersNames;
    for(auto p: qAsConst(peers)) {
        peersNames.append(p->name());
    }
    return peersNames;
}

void IvyQt::setFlushTimeout(int msec) {
    for(auto peer: qAsConst(peers)) {
        peer->setFlushTimeout(msec);
    }
}

void IvyQt::tcphandle() {
    // accept action
    auto tcp_socket = server->nextPendingConnection();
    newPeer(tcp_socket);
}

void IvyQt::udphandle() {
    while (udp_socket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(udp_socket->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;

        udp_socket->readDatagram(datagram.data(), datagram.size(),
                                &sender, &senderPort);
        QString msg = QString(datagram.data()).trimmed();
        // "<protocol number> <port> <???> <app name>"
        auto params = msg.split(" ");
        if(params[0] != "3") {
            qDebug() << "Protocol version not supported!";
            return;
        }
        quint16 tcp_port = params[1].toUInt();
        if(tcp_port == server->serverPort() && watcherId == params[2]) {
            //that's my own message, ignoring it.
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

    connect(peer, &Peer::ready,         this, &IvyQt::newPeerReady);
    connect(peer, &Peer::peerDied,      this, &IvyQt::deletePeer);
    connect(peer, &Peer::message,       this, [=](QString id, QStringList params) {
        int bindId = id.toInt();
        if(bindings.contains(bindId)) {
            bindings[bindId].callback(peer, params);
        }
    });
    connect(peer, &Peer::directMessage, this, [=](int identifier, QString params) {
        emit directMessage(peer, identifier, params);
    });
    connect(peer, &Peer::quitRequest, this, [=]() {
        emit quitRequest(peer);
    });

    peer->setFlushTimeout(flush_timeout);

    // send a synchro message and initial subscriptions.
    peer->sendId(server->serverPort(), name);
    for(auto &b: bindings) {
        peer->bind(b.bindId, b.regex);
    }
    peer->end_initial_bindings();
    peer->setInfoSent();

    peers.append(peer);
}

void IvyQt::newPeerReady(Peer* peer) {
    if(msgReady != "") {
        peer->sendMessage(msgReady);
    }

    emit peerReady(peer);
}

void IvyQt::deletePeer(Peer* peer) {
    auto peerName = peer->name();
    peers.removeAll(peer);
    peer->deleteLater();

    emit peerDied(peerName);

    // stopping properly so start can be called again.
    if(stopRequested && peers.length() == 0) {
        completeStop();
    }
}

void IvyQt::completeStop() {
    running = false;
    udp_socket->deleteLater();
    server->deleteLater();
    udp_socket = nullptr;
    server = nullptr;
    emit stopped();
}
