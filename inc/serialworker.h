#ifndef SERIALWORKER_H
#define SERIALWORKER_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>

class SerialWorker : public QObject
{
    Q_OBJECT
public:
    explicit SerialWorker(QObject *parent = nullptr);

    QStringList scanSerial();       // 扫描串口
    QStringList addbaud();          //波特率
    bool openSerial(QString port, int baud); // 打开
    void closeSerial();             // 关闭
    void sendData(QByteArray data); // 发送

signals:
    void dataReceived(QByteArray buf); // 收到数据
    void openSuccess();
    void openFailed();


private slots:
    void readData();

private:
    QSerialPort *serial;
};

#endif // SERIALWORKER_H