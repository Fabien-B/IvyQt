#include "peer.h"

Peer::Peer(QTcpSocket* tcp_socket, QObject* parent) :
    QObject(parent),
    peerId(""), appName(""), socket(tcp_socket), status(INIT)
{
    connect(tcp_socket, &QTcpSocket::readyRead, this, [=]() {
        rcv_array += tcp_socket->readAll();
        process();
    });

    connect(tcp_socket, &QTcpSocket::disconnected, this, [=]() {
        status = APP_QUIT;
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

    qDebug() << "type:" << type << "ident:" << ident << "params:" << params;

    if(status == INIT) {
        if(type == "0") {
            // Bye
            status = APP_QUIT;
        } else if(type == "1") {
            // Subscription
            qDebug() << "Received subscription before peer Id. Who are you ?";
        } else if(type == "2") {
            // Text message
            qDebug() << "Received text message before peer Id. Who are you ?";
        } else if(type == "3") {
            // Error
            qDebug() << "App " << appName << " encoutered error " << params;
        } else if(type == "4") {
            // Subscription deletion
            qDebug() << "Received message of type " << type << " before type 6!";
        } else if(type == "5") {
            // End of initial subscriptions.
            qDebug() << "Received Subscription deletion before peer Id. Who are you ?";
        } else if(type == "6") {
            //Peer ID
            appPort = ident.toUInt();
            peerId = peer_id(socket->peerAddress(), appPort);
            appName = params;
            status = SYNCHRONIZED;
        }

    } else if (status == SYNCHRONIZED) {
        if(type == "0") {
            // Bye
            status = APP_QUIT;
        } else if(type == "1") {
            // Subscription
            subscribe(ident, params);
        } else if(type == "2") {
            // Text message
            qDebug() << "don't you want to give your subs first ?";
        } else if(type == "3") {
            // Error
            qDebug() << "App " << appName << " encoutered error " << params;
        } else if(type == "4") {
            // Subscription deletion
            qDebug() << "don't you want to give your subs first ?";
        } else if(type == "5") {
            // End of initial subscriptions.
            status = INI_SUBS_OK;
        } else if(type == "6") {
            //Peer ID
            qDebug() << "I know you! Get out of here, you nasty boy!";
        }
    } else if (status == INI_SUBS_OK) {
        if(type == "0") {
            // Bye
            status = APP_QUIT;
        } else if(type == "1") {
            // Subscription
            subscribe(ident, params);
        } else if(type == "2") {
            // Text message
            handle_message(ident, params);
        } else if(type == "3") {
            // Error
            qDebug() << "App " << appName << " encoutered error " << params;
        } else if(type == "4") {
            // Subscription deletion
            unsubscribe(ident);
        } else if(type == "5") {
            // End of initial subscriptions.
            qDebug() << "It was already over! Go back to your bed now!";
        } else if(type == "6") {
            //Peer ID
        }
    } else if (status == APP_QUIT) {
        if(type == "0") {
            // Bye
            status = APP_QUIT;
        } else if(type == "1") {
            // Subscription
            qDebug() << "don't care";
        } else if(type == "2") {
            // Text message
            qDebug() << "don't care";
        } else if(type == "3") {
            // Error
            qDebug() << "App " << appName << " encoutered error " << params << "But I really don't care";
        } else if(type == "4") {
            // Subscription deletion
            qDebug() << "don't care";
        } else if(type == "5") {
            // End of initial subscriptions.
            qDebug() << "don't care";
        } else if(type == "6") {
            //Peer ID
            qDebug() << "don't care";
        }
    }
    else {
        qDebug() << "Status not handled ! THIS IS BAD!!! " << status;
    }
}

void Peer::subscribe(QString id, QString regex) {
    assert(!subscriptions.contains(id));
    subscriptions[id] = regex;
}

void Peer::unsubscribe(QString id) {
    assert(subscriptions.contains(id));
    subscriptions.remove(id);
}

void Peer::handle_message(QString id, QString params) {
    auto parameters = params.split(0x03);
    qDebug() << "message for regex " << id << "with params" << parameters;
    emit message(id, parameters);
}

void Peer::sendMessage(QString message) {
    // TODO very compilcated stuff
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
