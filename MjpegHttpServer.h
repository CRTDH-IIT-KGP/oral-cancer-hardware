#ifndef MJPEGHTTPSERVER_H
#define MJPEGHTTPSERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QImage>
#include <QMutex>
#include <QThread>
#include <QList>

class MjpegHttpServer : public QTcpServer
{
    Q_OBJECT

public:
    explicit MjpegHttpServer(QObject *parent = nullptr);
    bool start(quint16 port);
    void stop();
    void sendFrame(const QImage &image);

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private slots:
    void onClientDisconnected();

private:
    QList<QTcpSocket *> clients;
    QMutex clientsMutex;
};

#endif // MJPEGHTTPSERVER_H

