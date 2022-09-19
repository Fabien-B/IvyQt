#ifndef IVYQT_H
#define IVYQT_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <functional>
#include "peer.h"

struct Binding {
    int regexId;
    std::function<void(Peer* sender, QStringList params)> cb;
    QObject* context;
};

class IvyQt : public QObject
{
    Q_OBJECT
public:
    explicit IvyQt(QString name, QString msgReady = "", QObject *parent = nullptr);
    void start(QString domain, int udp_port);

    int bindMessage(QString regex, std::function<void(Peer*, QStringList)> callback);
    int bindMessage(QString regex, QObject* context, std::function<void(Peer*, QStringList)> callback);
    void unBindMessage(int bindId);
    void send(QString message);

    /**
     * @brief stop request. the "stopped" signal will be emitted once stop is effective.
     */
    void stop();

    /**
     * @brief getPeers
     * @return all current peers
     */
    QList<Peer*> getPeers();

    /**
     * @brief getPeer
     * @param name
     * @return peer with "name". nullptr if not found.
     */
    Peer* getPeer(QString name);

    /**
     * @brief setFlushTimeout set the timeout after which buffer will be flushed. Default to 0.
     * @param msec
     */
    void setFlushTimeout(int msec);

signals:
    void peerReady(Peer* peer);
    void directMessage(Peer* peer, int identifier, QString params);
    void quitRequest(Peer* peer);
    void peerDied(QString name);
    void stopped();

private slots:
    void tcphandle();
    void udphandle();
    void deletePeer(Peer* peer);
    void newPeerReady(Peer* peer);

private:

    void newPeer(QTcpSocket* socket);
    void completeStop();
    QString name;
    QString msgReady;

    QUdpSocket* udp_socket;
    QTcpServer* server;

    QList<Peer*> peers;

    QMap<int /*regexId*/, QString /*regex*/> regexes;

    QMap<int /*bindId*/, Binding> bindings;

    int nextRegexId;
    int nextBindId;

    QString watcherId;

    int flush_timeout;
    bool running;
    bool stopRequested;
};

#endif // IVYQT_H
