#ifndef DEVICEDATA_H
#define DEVICEDATA_H

#include <QString>
#include <qglobal.h>

// 通用配置
// 温湿度阈值(温度低于20，大于40为异常，湿度低于20高于80异常)
// 模拟温度范围是15~45，湿度15~85
const float TEMP_MIN = 20.0f;
const float TEMP_MAX = 40.0f;
const float HUMI_MIN = 20.0f;
const float HUMI_MAX = 80.0f;

// TCP 监听端口（全局唯一）
const quint16 TCP_PORT = 8080;

// 设备数据结构体
struct DeviceData
{
    float temp = 0.0f;         // 温度
    float humi = 0.0f;         // 湿度
    bool valid = false;         // 数据是否有效（设备是否在线）
    qint64 lastTime = 0;        // 最后一次收到数据的时间戳（毫秒）

    // 辅助函数：判断数据是否正常
    bool isNormal() const {
        return (temp >= TEMP_MIN && temp <= TEMP_MAX) &&
               (humi >= HUMI_MIN && humi <= HUMI_MAX);
    }
};

#endif // DEVICEDATA_H