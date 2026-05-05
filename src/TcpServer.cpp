#include "TcpServer.h"
#include <QDebug>
#include <QStringList>

TcpServer::TcpServer(QObject *parent) : QTcpServer(parent)
{
    connect(this, &QTcpServer::newConnection, this, &TcpServer::onNewConnection);
}

bool TcpServer::startServer(quint16 port)
{
    if (listen(QHostAddress::Any, port))
    {
        qDebug() << "TCP 服务启动成功，端口：" << port;
        return true;
    }
    else
    {
        qDebug() << "TCP 服务启动失败！";
        return false;
    }
}

void TcpServer::onNewConnection()
{
    QTcpSocket *socket = nextPendingConnection();
    connect(socket, &QTcpSocket::readyRead, this, &TcpServer::onClientDataReady);
    connect(socket, &QTcpSocket::disconnected, this, &TcpServer::onClientDisconnected);
    emit newClientConnected(socket);
}

void TcpServer::onClientDataReady()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;

    while (socket->canReadLine())
    {
        QByteArray line = socket->readLine().trimmed();
        if (line.isEmpty()) continue;

        QString msg = QString::fromLocal8Bit(line);
        QStringList parts = msg.split(':');
        if (parts.size() != 2) continue;

        QString devID = parts[0];
        QStringList values = parts[1].split(',');
        if (values.size() != 2) continue;

        bool ok1, ok2;
        float temp = values[0].toFloat(&ok1);
        float humi = values[1].toFloat(&ok2);
        if (ok1 && ok2)
        {
            emit dataReceived(devID, temp, humi);
            qDebug() << "收到数据 - 设备:" << devID << "温度:" << temp << "湿度:" << humi;
        }
    }
}

void TcpServer::onClientDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;
    qDebug() << "客户端断开连接：" << socket->peerAddress().toString();
    socket->deleteLater();
}