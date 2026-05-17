QT += widgets serialport network sql

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


INCLUDEPATH += $$PWD/inc

SOURCES += \
    $$PWD/src/main.cpp \
    $$PWD/src/mainwindow.cpp \
    $$PWD/src/serialworker.cpp \
    $$PWD/src/video.cpp \
    src/TcpServer.cpp

HEADERS += \
    $$PWD/inc/mainwindow.h \
    $$PWD/inc/serialworker.h \
    $$PWD/inc/video.h \
    inc/TcpServer.h \
    inc/devicedata.h

FORMS += mainwindow.ui

# ================= FFmpeg 7.1.1 配置 =================
FFMPEG_DIR = D:/Downloads/ffmpeg-7.1.1-full_build-shared/ffmpeg-7.1.1-full_build-shared

DEFINES += __STDC_CONSTANT_MACROS

INCLUDEPATH += $$FFMPEG_DIR/include
LIBS += -L$$FFMPEG_DIR/lib \
        -lavformat -lavcodec -lavutil -lswscale


# 选择硬件加速平台,根据目标平台注释/取消注释
DEFINES += USE_D3D11VA      # Windows D3D11VA
# Windows D3D11VA 硬件加速必须的系统库
LIBS += -ld3d11 -ldxva2 -ld3dcompiler

# DEFINES += USE_VAAPI      # Linux VA-API
#LIBS += -lva -lva-dr



# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
