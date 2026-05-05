#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QSqlQuery>
#include <QMap>
#include <QSet>
#include "DeviceData.h"
#include "TcpServer.h"
#include "video.h"
#include "SerialWorker.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void addLog(const QString &msg);
    void addLog(const QString &msg, const QString &color);

private slots:
    void updateSystemTime();
    void onTcpDataReceived(const QString &devID, float temp, float humi);
    void onNewClientConnected(QTcpSocket* client); // 新增：新客户端连接，保存socket
    void checkDataTimeout();
    void updateDisplayForDevice(const QString &devID);
    void updateStatusLight(bool isNormal, bool isWorking);
    void insertAlarmLog(const QString &content, const QString &status);
    void switchPendingDisplay(); // 新增：切换待显示的设备

private:
    Ui::MainWindow *ui;

    Video *m_video;
    SerialWorker *m_serial;
    TcpServer *m_tcpServer;

    // 核心运行状态控制（stop/start用）
    bool m_isRunning;
    // 所有连接的下位机客户端socket（用于发启停指令）
    QSet<QTcpSocket*> m_clientSockets;

    // 解决刷新太快的显示控制
    QTimer *m_displayLockTimer;
    bool m_isDisplayLocked; // 显示锁定，避免毫秒级切换
    QString m_pendingDevID; // 待显示的设备ID

    QMap<QString, DeviceData> m_devicesData;
    QString m_currentShowDevID;

    QTimer *m_timeoutTimer;
};

#endif // MAINWINDOW_H