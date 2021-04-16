#include "peer.h"

Peer::Peer(QTcpSocket* tcp_socket, QObject* parent) :
    QObject(parent),
    peerId(""), appName(""), socket(tcp_socket), state(INIT),
    info_sent(false), info_rcv(false)
{
    connect(socket, &QTcpSocket::readyRead, this, [=]() {
        rcv_array += tcp_socket->readAll();
        process();
    });

    connect(socket, &QTcpSocket::disconnected, this, [=]() {
        state = APP_QUIT;
        emit peerDied(this);
    });
}

Peer::~Peer() {
    socket->deleteLater();
}

void Peer::process() {
    if(rcv_array.contains('\n')) {
        int bytes = rcv_array.indexOf('\n') + 1;       // Find the end of message
        QByteArray message = rcv_array.left(bytes);    // Cut the message
        rcv_array = rcv_array.mid(bytes);     // Keep the data read too early

        parseMessage(message);                        // process message

        process();                                    // Call itself again to process remaining messages
    }
}

void Peer::parseMessage(QByteArray message) {
    assert(message.at(1) == ' ');
    assert(message.at(message.length()-1) == '\n');

    auto type = message.left(1);
    int stx_pos = message.indexOf(0x02);       // Find 0x02
    auto ident = message.mid(2, stx_pos-2);
    auto params=message.mid(stx_pos+1, message.length() - stx_pos - 2);

    if(type == "0") {
        // Bye
        stop();
        state = APP_QUIT;
    } else if(type == "1") {
        // Subscription
        if(state == SYNCHRONIZED || state == INI_SUBS_OK) {
            subscribe(ident, params);
        } else {
            qDebug() << "Received subscription while in state " << state;
        }
    } else if(type == "2") {
        // Text message
        if (state == INI_SUBS_OK) {
            handle_message(ident, params);
        } else {
            qDebug() << "Received text message while in state " << state;
        }
    } else if(type == "3") {
        // Error
        qDebug() << "App " << appName << " encoutered error " << params;
    } else if(type == "4") {
        // Subscription deletion
        if(state == INI_SUBS_OK) {
            unsubscribe(ident);
        } else {
            qDebug() << "Received subscription deletion while in state " << state;
        }
    } else if(type == "5") {
        // End of initial subscriptions.
        if (state == SYNCHRONIZED) {
            state = INI_SUBS_OK;
            info_rcv = true;
            if(info_sent) {
                emit ready(this);
            }
        } else {
            qDebug() << "Received endOfSub while in state " << state;
        }
    } else if(type == "6") {
        if(state == INIT) {
            //Peer ID
            appPort = ident.toUInt();
            peerId = peer_id(socket->peerAddress(), appPort);
            appName = params;
            state = SYNCHRONIZED;
        } else {
            qDebug() << "Received peerID while in state " << state;
        }
    }
}

void Peer::subscribe(QString id, QString regex) {
    assert(!subscriptions.contains(id));
    subscriptions[id] = QRegularExpression(regex);
}

void Peer::unsubscribe(QString id) {
    assert(subscriptions.contains(id));
    subscriptions.remove(id);
}

void Peer::handle_message(QString id, QString params) {
    auto parameters = params.split(0x03);
    emit message(id, parameters);
}

void Peer::stop() {
    if(socket) {
        socket->disconnectFromHost();
    }
}

void Peer::sendMessage(QString message) {
    for(auto i=subscriptions.begin(); i!=subscriptions.end(); ++i) {
        qDebug() << "test pattern " << i.value().pattern();
        auto match = i.value().match(message);
        if(match.hasMatch()) {
            auto caps = match.capturedTexts();
            // remove the implicit capturing group number 0, capturing the substring matched by the entire pattern
            caps.removeAt(0);
            qDebug() << "matched with " << caps;
            QString params = caps.join(0x03);
            QString ident = i.key();
            send_data(2, ident, params);
        }
    }
}


void Peer::sendBye() {
    send_data(0, "0", "");
}

void Peer::bind(int bindId, QString regex) {
    send_data(1, QString::number(bindId), regex);
}


void Peer::unBind(int bindId) {
    send_data(4, QString::number(bindId), "");
}


void Peer::end_initial_bindings() {
    send_data(5, "0", "");
}

void Peer::sendId(quint16 port, QString myName) {
    send_data(6, QString::number(port), myName);
}

void Peer::send_data(int type, QString ident, QString params) {
    QByteArray data;
    data += QString::number(type) + " ";
    data += ident;
    data += 0x02;
    data += params;
    data += "\n";
    socket->write(data);
    socket->flush();
}

QString Peer::peer_id(QHostAddress addr, quint16 port) {
    return addr.toString() + ":" + QString::number(port);
}
