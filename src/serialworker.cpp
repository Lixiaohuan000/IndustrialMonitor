#include "SerialWorker.h"

SerialWorker::SerialWorker(QObject *parent) : QObject(parent)
{
    serial = new QSerialPort(this);
    connect(serial, &QSerialPort::readyRead, this, &SerialWorker::readData);
}

QStringList SerialWorker::scanSerial()
{
    QStringList portList;
    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts())
    {
        portList << info.portName();
    }
    return portList;
}

QStringList SerialWorker::addbaud()
{
    QStringList baudList;
    baudList << "9600" << "19200" << "38400" << "57600" << "115200";
    return baudList;
}

bool SerialWorker::openSerial(QString port, int baud)
{
    serial->setPortName(port);
    serial->setBaudRate(baud);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);

    bool ok = serial->open(QIODevice::ReadWrite);
    if (ok)
        emit openSuccess();
    else
        emit openFailed();
    return ok;
}

void SerialWorker::closeSerial()
{
    if (serial->isOpen())
        serial->close();
}

void SerialWorker::sendData(QByteArray data)
{
    if (serial->isOpen())
        serial->write(data);
}

void SerialWorker::readData()
{
    QByteArray buf = serial->readAll();
    emit dataReceived(buf);
}