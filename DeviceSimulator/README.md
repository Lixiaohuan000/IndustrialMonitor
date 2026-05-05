# DeviceSimulator - 设备模拟器
## 项目简介
这是一个基于Qt的TCP设备模拟器，用于给 `IndustrialMonitor` 工控上位机发送模拟温湿度数据，支持自动重连、启停指令控制。

## 核心功能
-  TCP客户端，连接上位机服务端
-  自动重连，失败指数退避
-  响应上位机的 START_SEND / STOP_SEND 指令
-  定时发送随机温湿度数据

## 编译运行
1. 用Qt Creator打开 `DeviceSimulator.pro`
2. 修改代码里的服务器地址为上位机IP
3. 编译运行即可