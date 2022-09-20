#include <IvyQt/ivyqt.h>
#include <QUdpSocket>
#include <QRandomGenerator>

IvyQt::IvyQt(QString name, QString msgReady, QObject *parent) :
    QObject(parent),
    name(name), msgReady(msgReady),
    udp_socket(nullptr), server(nullptr),
    nextRegexId(0), nextBindId(0), flush_timeout(0),
    running(false), stopRequested(false)
{

}

void IvyQt::start(QString domain, int udp_port) {
    if(running) {
        return;
    }
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
    auto data = QString("3 %1 %2 %3\n").arg(QString::number(server->serverPort()), watcherId, name).toUtf8();
    udp_socket->writeDatagram(data, QHostAddress(domain), udp_port);

    running = true;
}

int IvyQt::bindMessage(QString regex, QObject* context, std::function<void(Peer*, QStringList)> callback) {

    auto existing = std::find_if(regexes.begin(), regexes.end(),
        [=](QString reg){
            return reg == regex;
        });

    int regexId;

    if(existing == regexes.end()) {
        // this regex is not used yet
        //qDebug() << "new regex:" << regex;
        regexId = nextRegexId;
        nextRegexId += 1;
        regexes[regexId] = regex;

        // publish regex to all peers
        for(auto peer: qAsConst(peers)) {
            peer->bind(regexId, regex);
        }

    } else {
        //qDebug() << "regex exist:" << regex << existing.key();
        regexId = existing.key();
    }


    Binding b = {
        regexId,
        callback,
        context,
    };
    int bindId = nextBindId;
    bindings[bindId] = b;
    nextBindId += 1;


    if(context != nullptr) {
        connect(context, &QObject::destroyed, this, [=](QObject* obj) {
            // obj is about to be destroyed
            // remove all bindings that have obj as context object
            QList<int> bindIdsDel;
            for(auto it=bindings.constBegin(); it!=bindings.constEnd(); it++) {
                if(it.value().context == obj) {
                    bindIdsDel.append(it.key());
                }
            }

            for(auto bindId: bindIdsDel) {
                unBindMessage(bindId);
            }
        });
    }

    return bindId;
}

int IvyQt::bindMessage(QString regex, std::function<void(Peer*, QStringList)> callback) {
    return bindMessage(regex, nullptr, callback);
}

void IvyQt::unBindMessage(int bindId) {
    // Schedule the unbind for the next event loop
    // If unBindMessagePrivate is called in a callback,
    // "bindings" is modified while iterating over it, causing the program to crash
    QTimer::singleShot(0, [=]{unBindMessagePrivate(bindId);});
}

void IvyQt::unBindMessagePrivate(int bindId) {
    // obj is about to be destroyed
    // remove all bindings that have obj as context object
    bindings.remove(bindId);

    // clear unsued regexes
    QMutableMapIterator<int, QString> reg(regexes);
    while (reg.hasNext()) {
        reg.next();

        bool regex_used = std::any_of(bindings.begin(), bindings.end(),
            [=](Binding bd){
                return bd.regexId == reg.key();
            });
        if(!regex_used) {
            for(auto peer: qAsConst(peers)) {
                // no more callbacks to this regex. Advise all peers to remove this regex.
                peer->unBind(reg.key());
            }
            //remove regex
            reg.remove();
        }
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

QList<Peer*> IvyQt::getPeers() {
    return peers;
}

Peer* IvyQt::getPeer(QString name) {
    for(auto peer: qAsConst(peers)) {
        if(peer->name() == name) {
            return peer;
        }
    }
    return nullptr;
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
        int regexId = id.toInt();
        for(auto &binding: bindings) {
            if(binding.regexId == regexId) {
                binding.cb(peer, params);
            }
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
        peer->bind(b.regexId, regexes[b.regexId]);
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
