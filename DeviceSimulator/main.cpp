#include <QCoreApplication>
#include "device.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    // 启动两台设备，连接到本地 127.0.0.1:8080
    new Device("DEV1", "127.0.0.1", 8080);
    new Device("DEV2", "127.0.0.1", 8080);

    return a.exec();
}