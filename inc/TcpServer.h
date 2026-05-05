#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QObject>
// 引入通用数据定义（但不持有数据）
#include "DeviceData.h"

class TcpServer : public QTcpServer
{
    Q_OBJECT
public:
    explicit TcpServer(QObject *parent = nullptr);
    // 启动服务
    bool startServer(quint16 port = TCP_PORT);

private slots:
    void onNewConnection();
    void onClientDataReady();
    void onClientDisconnected();

signals:
    // 只发解析好的原始数据，不发结构体
    void dataReceived(QString devID, float temp, float humi);

    void newClientConnected(QTcpSocket* client);
};

#endif // TCPSERVER_H