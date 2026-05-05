#ifndef DEVICE_H
#define DEVICE_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QQueue>

class Device : public QObject
{
    Q_OBJECT
public:
    explicit Device(const QString &devID, const QString &serverHost, quint16 serverPort, QObject *parent = nullptr);
    ~Device();

private slots:
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);
    void sendData();
    void flushSendQueue();
    // 接收上位机指令
    void onServerCommandReceived();

private:
    void doConnect();
    void scheduleReconnect();

    QString m_devID;
    QString m_serverHost;
    quint16 m_serverPort;

    QTcpSocket *m_socket;
    QTimer m_sendTimer;
    QTimer m_reconnectTimer;
    QQueue<QByteArray> m_sendQueue;

    int m_reconnectDelay;
    int m_maxReconnectDelay;

    // 发送使能开关（用于响应上位机stop/start）
    bool m_isSendEnabled;
};

#endif // DEVICE_H