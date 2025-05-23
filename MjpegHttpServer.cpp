
#include "MjpegHttpServer.h"
#include <QBuffer>
#include <QDateTime>

MjpegHttpServer::MjpegHttpServer(QObject *parent) : QTcpServer(parent) {}

bool MjpegHttpServer::start(quint16 port) {
    if (!this->listen(QHostAddress::Any, port)) {
        qWarning() << "MJPEG server failed to start on port" << port << ":" << this->errorString();
        return false;
    }

    qDebug() << "MJPEG server started on port" << port;
    return true;
}


void MjpegHttpServer::stop()
{
    close();
    QMutexLocker locker(&clientsMutex);
    for (QTcpSocket *client : clients) {
        client->disconnectFromHost();
    }
    clients.clear();
}

void MjpegHttpServer::incomingConnection(qintptr socketDescriptor)
{
    QTcpSocket *client = new QTcpSocket(this);
    client->setSocketDescriptor(socketDescriptor);

    connect(client, &QTcpSocket::disconnected, this, &MjpegHttpServer::onClientDisconnected);

    // Send HTTP MJPEG headers
    QByteArray header;
    header.append("HTTP/1.0 200 OK\r\n");
    header.append("Server: ThermalCamServer\r\n");
    header.append("Content-Type: multipart/x-mixed-replace; boundary=frame\r\n");
    header.append("Cache-Control: no-cache\r\n");
    header.append("Pragma: no-cache\r\n\r\n");
    client->write(header);
    client->flush();

    QMutexLocker locker(&clientsMutex);
    clients.append(client);
}

void MjpegHttpServer::onClientDisconnected()
{
    QTcpSocket *client = qobject_cast<QTcpSocket *>(sender());
    if (client) {
        QMutexLocker locker(&clientsMutex);
        clients.removeAll(client);
        client->deleteLater();
    }
}

void MjpegHttpServer::sendFrame(const QImage &image)
{
    QByteArray jpegData;
    QBuffer buffer(&jpegData);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "JPEG");

    QByteArray frame;
    frame.append("--frame\r\n");
    frame.append("Content-Type: image/jpeg\r\n");
    frame.append("Content-Length: " + QByteArray::number(jpegData.size()) + "\r\n\r\n");
    frame.append(jpegData);
    frame.append("\r\n");

    QMutexLocker locker(&clientsMutex);
    for (QTcpSocket *client : clients) {
        if (client->state() == QAbstractSocket::ConnectedState) {
            client->write(frame);
            client->flush();
        }
    }
}
