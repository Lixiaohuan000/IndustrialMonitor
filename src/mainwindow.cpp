#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDateTime>
#include <QTextCursor>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QCoreApplication>
#include <QStringConverter>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    m_video = new Video(this);
    m_serial = new SerialWorker(this);

    // 初始化运行状态：默认启动就运行
    m_isRunning = true;
    m_isDisplayLocked = false;
    m_pendingDevID = "";

    // 显示锁定定时器：解决刷新太快，每个设备至少显示500ms
    m_displayLockTimer = new QTimer(this);
    m_displayLockTimer->setSingleShot(true); // 单次触发
    connect(m_displayLockTimer, &QTimer::timeout, this, &MainWindow::switchPendingDisplay);

    // TCP 服务器启动
    m_tcpServer = new TcpServer(this);
    bool tcpStartOk = m_tcpServer->startServer(TCP_PORT);
    if (tcpStartOk)
    {
        addLog("系统启动成功，TCP 服务器监听端口 8080");
        ui->btn_start->setEnabled(false);
        ui->btn_stop->setEnabled(true);
        ui->btn_reset->setEnabled(true);
    }
    else
    {
        addLog("TCP 服务器启动失败！端口8080可能被占用", "red");
    }
    // 绑定TCP信号
    connect(m_tcpServer, &TcpServer::dataReceived, this, &MainWindow::onTcpDataReceived);
    connect(m_tcpServer, &TcpServer::newClientConnected, this, &MainWindow::onNewClientConnected);

    // 超时检测定时器
    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setInterval(5000);
    connect(m_timeoutTimer, &QTimer::timeout, this, &MainWindow::checkDataTimeout);
    m_timeoutTimer->start();

    // 系统时间定时器
    QTimer *systemTimer = new QTimer(this);
    connect(systemTimer, &QTimer::timeout, this, &MainWindow::updateSystemTime);
    systemTimer->start(1000);
    updateSystemTime();

    // 串口控件初始化
    ui->comboBox_port->clear();
    ui->comboBox_port->addItems(m_serial->scanSerial());
    ui->comboBox_baud->clear();
    ui->comboBox_baud->addItems(m_serial->addbaud());

    // 按钮初始状态
    ui->btn_closeSerial->setEnabled(false);

    // 视频画面显示
    connect(m_video, &Video::frameReady, this, [this](const QImage& img) {
        QPixmap pix = QPixmap::fromImage(img);
        ui->videoLabel->setPixmap(pix.scaled(ui->videoLabel->size(),
                                             Qt::KeepAspectRatio,
                                             Qt::SmoothTransformation));
        ui->videoLabel->setAlignment(Qt::AlignCenter);
    });

    // 串口信号处理
    connect(m_serial, &SerialWorker::dataReceived, this, [this](QByteArray buf){
        addLog("串口数据：" + QString::fromUtf8(buf));
    });

    connect(m_serial, &SerialWorker::openSuccess, this, [this](){
        addLog("串口打开成功");
        ui->btn_openSerial->setEnabled(false);
        ui->btn_closeSerial->setEnabled(true);
        ui->comboBox_port->setEnabled(false);
        ui->comboBox_baud->setEnabled(false);
    });

    connect(m_serial, &SerialWorker::openFailed, this, [this](){
        addLog("串口打开失败", "red");
    });

    // 视频控制按钮
    connect(ui->btn_play, &QPushButton::clicked, this, [this](){
        addLog("开启实时监控");
        //m_video->open("D:/QTproject/player/videos/test.mp4");
        m_video->open("https://www.w3schools.com/html/mov_bbb.mp4");
        m_video->start();
    });

    connect(ui->btn_stopvideo, &QPushButton::clicked, this, [this](){
        m_video->stop();
        addLog("关闭监控");
    });

    //reset按钮逻辑
    connect(ui->btn_reset, &QPushButton::clicked, this, [this](){
        //清空所有设备的存储数据
        m_devicesData.clear();
        m_currentShowDevID = "";
        m_pendingDevID = "";
        m_isDisplayLocked = false;

        // 恢复运行状态
        if (!m_isRunning)
        {
            m_isRunning = true;
            ui->btn_start->setEnabled(false);
            ui->btn_stop->setEnabled(true);
        }

        // 给所有下位机发送重新开始发送的指令
        for (QTcpSocket* client : m_clientSockets)
        {
            if (client->state() == QTcpSocket::ConnectedState)
            {
                client->write("START_SEND\n");
                client->flush();
            }
        }

        // 刷新UI到待机状态
        updateDisplayForDevice("");
        addLog("已重置所有数据，恢复接收与显示");
    });

    // 核心修复：start按钮逻辑
    connect(ui->btn_start, &QPushButton::clicked, this, [this](){
        if (m_isRunning) return;
        m_isRunning = true;
        // 给所有下位机发送启动发送指令
        for (QTcpSocket* client : m_clientSockets)
        {
            if (client->state() == QTcpSocket::ConnectedState)
            {
                client->write("START_SEND\n");
                client->flush();
            }
        }
        addLog("已恢复数据接收与显示");
        ui->btn_start->setEnabled(false);
        ui->btn_stop->setEnabled(true);
    });

    // stop按钮逻辑
    connect(ui->btn_stop, &QPushButton::clicked, this, [this](){
        if (!m_isRunning)
            return;
        m_isRunning = false;
        // 给所有下位机发送停止发送指令
        for (QTcpSocket* client : m_clientSockets)
        {
            if (client->state() == QTcpSocket::ConnectedState) {
                client->write("STOP_SEND\n");
                client->flush();
            }
        }
        addLog("已停止数据接收与显示", "orange");
        ui->btn_start->setEnabled(true);
        ui->btn_stop->setEnabled(false);
    });

    // 串口控制按钮
    connect(ui->btn_openSerial, &QPushButton::clicked, this, [this](){
        QString port = ui->comboBox_port->currentText();
        int baud = ui->comboBox_baud->currentText().toInt();
        m_serial->openSerial(port, baud);
        addLog(QString("尝试打开串口：%1 波特率：%2").arg(port).arg(baud));
    });

    connect(ui->btn_closeSerial, &QPushButton::clicked, this, [this](){
        m_serial->closeSerial();
        addLog("串口已关闭");
        ui->btn_openSerial->setEnabled(true);
        ui->btn_closeSerial->setEnabled(false);
        ui->comboBox_port->setEnabled(true);
        ui->comboBox_baud->setEnabled(true);
    });

    // 初始化UI显示
    m_currentShowDevID = "无";
    updateDisplayForDevice("");
}

// 保存新连接的客户端socket，用于发启停指令
void MainWindow::onNewClientConnected(QTcpSocket* client)
{
    m_clientSockets.insert(client);
    // 客户端断开时，从集合中移除
    connect(client, &QTcpSocket::disconnected, this, [this, client](){
        m_clientSockets.remove(client);
    });
    // 刚连接的客户端，同步当前运行状态
    if (m_isRunning)
    {
        client->write("START_SEND\n");
    }
    else
    {
        client->write("STOP_SEND\n");
    }
    client->flush();
}

//解决刷新太快的显示逻辑
void MainWindow::onTcpDataReceived(const QString &devID, float temp, float humi)
{
    // 停止状态下，不处理任何数据
    if (!m_isRunning) return;

    // 更新设备数据存储
    DeviceData &data = m_devicesData[devID];
    data.temp = temp;
    data.humi = humi;
    data.valid = true;
    data.lastTime = QDateTime::currentMSecsSinceEpoch();

    // 显示未锁定：直接刷新UI
    if (!m_isDisplayLocked)
    {
        m_currentShowDevID = devID;
        updateDisplayForDevice(devID);
        // 锁定显示500ms，期间不切换
        m_isDisplayLocked = true;
        m_displayLockTimer->start(500);
    }
    // 显示已锁定：缓存待切换的设备
    else
    {
        m_pendingDevID = devID;
    }

    // 异常判断+日志+数据库写入
    if (!data.isNormal())
    {
        // 拼接完整的异常警告信息：设备 + 温度 + 湿度
        QString alarmContent = QString("[%1 异常] 温度:%2℃ 湿度:%3%")
                                   .arg(devID)
                                   .arg(temp, 0, 'f', 1)
                                   .arg(humi, 0, 'f', 1);

        addLog(alarmContent, "red");
        insertAlarmLog(alarmContent, "异常");
    }
    else
    {
        addLog(QString("[%1 正常] 温度:%2℃ 湿度:%3%").arg(devID).arg(temp,0,'f',1).arg(humi,0,'f',1));
    }
}

// 锁定时间到，切换待显示的设备
void MainWindow::switchPendingDisplay()
{
    m_isDisplayLocked = false;
    // 有待切换的设备，刷新UI
    if (!m_pendingDevID.isEmpty() && m_pendingDevID != m_currentShowDevID)
    {
        m_currentShowDevID = m_pendingDevID;
        updateDisplayForDevice(m_pendingDevID);
    }
    m_pendingDevID = "";
}

// 数据超时检测
void MainWindow::checkDataTimeout()
{
    // 停止状态下，不检测超时
    if (!m_isRunning)
        return;

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (auto it = m_devicesData.begin(); it != m_devicesData.end(); ++it)
    {
        if (it->valid && (now - it->lastTime) > 30000)
        {
            it->valid = false;
            addLog(QString("设备 %1 数据超时（>30秒无数据），状态离线").arg(it.key()), "orange");
            if (it.key() == m_currentShowDevID) {
                updateDisplayForDevice(m_currentShowDevID);
            }
        }
    }
}

// 更新UI显示
void MainWindow::updateDisplayForDevice(const QString &devID)
{
    if (devID.isEmpty() || !m_devicesData.contains(devID) || !m_devicesData[devID].valid)
    {
        ui->lbl_Device->setText("无");
        ui->lbl_TempValue->setText("0");
        ui->lbl_HumidityValue->setText("0");
        ui->lbl_WorkStatusValue->setText("待机");
        updateStatusLight(false, false);
        return;
    }

    DeviceData &data = m_devicesData[devID];
    ui->lbl_Device->setText(devID);
    ui->lbl_TempValue->setText(QString::number(data.temp, 'f', 1));
    ui->lbl_HumidityValue->setText(QString::number(data.humi, 'f', 1));
    ui->lbl_WorkStatusValue->setText(data.isNormal() ? "正常" : "异常");
    updateStatusLight(data.isNormal(), true);
}

// 状态灯更新
void MainWindow::updateStatusLight(bool isNormal, bool isWorking)
{
    if (!isWorking)
    {
        ui->lightLabel->setStyleSheet(R"(
            background-color: #888888;
            border-radius: 20px;
            min-width:40px; min-height:40px; max-width:40px; max-height:40px;
        )");
    }
    else if (isNormal)
    {
        ui->lightLabel->setStyleSheet(R"(
            background-color: green;
            border-radius: 20px;
            min-width:40px; min-height:40px; max-width:40px; max-height:40px;
        )");
    }
    else
    {
        ui->lightLabel->setStyleSheet(R"(
            background-color: red;
            border-radius: 20px;
            min-width:40px; min-height:40px; max-width:40px; max-height:40px;
        )");
    }
}

// 单参数addLog
void MainWindow::addLog(const QString &msg)
{
    addLog(msg, "black");
}

// 双参数addLog
void MainWindow::addLog(const QString &msg, const QString &color)
{
    QString time = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    QString log = QString("<span style='color:%1;'>[%2] %3</span>").arg(color).arg(time).arg(msg);
    ui->logText->append(log);
    ui->logText->moveCursor(QTextCursor::End);
}

// 系统时间更新
void MainWindow::updateSystemTime()
{
    ui->lbl_SystemTimeValue->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
}

// 异常警告写入数据库+CSV
void MainWindow::insertAlarmLog(const QString &content, const QString &status)
{
    // 写入数据库
    QSqlQuery query;
    query.prepare("INSERT INTO alarm_log (time, content, status) VALUES (?,?,?)");

    QString nowTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    query.addBindValue(nowTime);
    query.addBindValue(content);   // [dev1异常] 温度:xx 湿度:xx
    query.addBindValue(status);
    query.exec();

    // 写入CSV
    QString excelPath = QCoreApplication::applicationDirPath() + "/设备告警记录.csv";
    QFile file(excelPath);

    if (!file.exists())
    {
        if (file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QTextStream out(&file);
            out.setEncoding(QStringConverter::System);
            out << "\"时间\",\"告警内容\",\"状态\"\n";
            file.close();
        }
    }

    if (file.open(QIODevice::Append | QIODevice::Text))
    {
        QTextStream out(&file);
        out.setEncoding(QStringConverter::System);
        out << "\"" << nowTime << "\",\"" << content << "\",\"" << status << "\"\n";
        file.close();
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}