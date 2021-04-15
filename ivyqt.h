#ifndef IVYQT_H
#define IVYQT_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <functional>
#include "peer.h"

struct Binding {
    QString regex;
    std::function<void(QStringList)> callback;
    int bindId;
};

class IvyQt : public QObject
{
    Q_OBJECT
public:
    explicit IvyQt(QString name, QObject *parent = nullptr);
    void start(QString domain, int udp_port);

    int bindMessage(QString regex, std::function<void(QStringList)> callback);
    void unBindMessage(int bindId);
    void send(QString message);
    void stop();

signals:

private slots:
    void tcphandle();
    void udphandle();
    void handleMessage(QString id, QStringList params);
    void deletePeer(Peer* peer);

private:

    void newPeer(QTcpSocket* socket);
    QString name;

    QUdpSocket* udp_socket;
    QTcpServer* server;

    QList<Peer*> peers;

    QMap<int /*bindId*/, Binding> bindings;
    int nextBindId;
};

#endif // IVYQT_H
