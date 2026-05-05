#include "device.h"
#include <QRandomGenerator>
#include <QDebug>
#include <QDateTime>

Device::Device(const QString &devID, const QString &serverHost, quint16 serverPort, QObject *parent)
    : QObject(parent)
    , m_devID(devID)
    , m_serverHost(serverHost)
    , m_serverPort(serverPort)
    , m_reconnectDelay(1000)
    , m_maxReconnectDelay(30000)
    , m_isSendEnabled(true) //    默认启动就允许发送
{
    m_socket = new QTcpSocket(this);

    // 连接成功
    connect(m_socket, &QTcpSocket::connected, this, &Device::onConnected);
    // 连接断开
    connect(m_socket, &QTcpSocket::disconnected, this, &Device::onDisconnected);
    // 错误发生
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &Device::onError);
    //    接收上位机指令
    connect(m_socket, &QTcpSocket::readyRead, this, &Device::onServerCommandReceived);

    // 发送定时器，但先不启动（等连接成功后再启动）
    connect(&m_sendTimer, &QTimer::timeout, this, &Device::sendData);

    // 开始连接
    doConnect();
}

Device::~Device()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState)
        m_socket->disconnectFromHost();
}

void Device::onConnected()
{
    // 重置重连延迟
    m_reconnectDelay = 1000;
    // 连接成功后启动发送计时器
    if (!m_sendTimer.isActive())
        m_sendTimer.start(1000);
    // 补发断网期间积压的数据
    flushSendQueue();
}

void Device::onDisconnected()
{
    qDebug() << "[" << m_devID << "] 断开连接，将在" << m_reconnectDelay << "ms后重试...";
    // 断开时停止发送，避免无效写入
    m_sendTimer.stop();
    scheduleReconnect();
}

void Device::onError(QAbstractSocket::SocketError error)
{
    qDebug() << "[" << m_devID << "] Socket错误:" << error;
    if (m_socket->state() != QAbstractSocket::ConnectedState && !m_reconnectTimer.isActive())
        scheduleReconnect();
}

//    接收上位机的 STOP_SEND / START_SEND 指令
void Device::onServerCommandReceived()
{
    while (m_socket->canReadLine()) {
        QByteArray cmd = m_socket->readLine().trimmed();
        if (cmd == "STOP_SEND") {
            m_isSendEnabled = false;
            qDebug() << "[" << m_devID << "] 收到上位机指令：停止发送";
        } else if (cmd == "START_SEND") {
            m_isSendEnabled = true;
            qDebug() << "[" << m_devID << "] 收到上位机指令：恢复发送";
        }
    }
}

void Device::sendData()
{
    //    停止状态下，不发送数据
    if (!m_isSendEnabled)
        return;

    // 未连接，数据由 flushSendQueue 在重连后补发
    if (m_socket->state() != QAbstractSocket::ConnectedState)
        return;

    // 生成模拟数据：温度 15~45℃，湿度 15~85%
    float temp = 15.0f + QRandomGenerator::global()->generateDouble() * 30.0f;
    float humi = 15.0f + QRandomGenerator::global()->generateDouble() * 70.0f;

    QString data = QString("%1:%2,%3\n")
                       .arg(m_devID)
                       .arg(temp, 0, 'f', 1)
                       .arg(humi, 0, 'f', 1);
    QByteArray packet = data.toLocal8Bit();

    qint64 written = m_socket->write(packet);
    if (written == packet.size())
    {
        m_socket->flush();
        qDebug() << "[" << m_devID << "] 发送 >>>" << data;
    }
    else
    {
        // 写入失败（如socket突然错误），加入发送队列
        qDebug() << "[" << m_devID << "] 写入失败，加入发送队列";
        m_sendQueue.enqueue(packet);
    }
}

void Device::flushSendQueue()
{
    if (m_socket->state() != QAbstractSocket::ConnectedState)
        return;
    while (!m_sendQueue.isEmpty())
    {
        QByteArray packet = m_sendQueue.dequeue();
        qint64 written = m_socket->write(packet);
        if (written != packet.size())
        {
            // 发送失败，重新放回队列头部，等待下次再试
            m_sendQueue.prepend(packet);
            break;
        }
        m_socket->flush();
        qDebug() << "[" << m_devID << "] 重发队列数据 >>>" << packet;
    }
}

void Device::doConnect()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState ||
        m_socket->state() == QAbstractSocket::ConnectingState)
    {
        return;  // 已连接或正在连接，不做重复操作
    }
    m_socket->connectToHost(m_serverHost, m_serverPort);
}

void Device::scheduleReconnect()
{
    if (m_reconnectTimer.isActive())
        return;

    m_reconnectTimer.singleShot(m_reconnectDelay, this, [this]() {
        qDebug() << "[" << m_devID << "] 正在尝试重连...";
        doConnect();
        // 延迟翻倍，但不超过最大延迟
        m_reconnectDelay = qMin(m_reconnectDelay * 2, m_maxReconnectDelay);
    });
}