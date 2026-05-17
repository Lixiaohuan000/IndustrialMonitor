#undef NV12//解决MSVC下NV12宏冲突问题

#include "video.h"
#include <QElapsedTimer>
#include <QDebug>
#include <memory>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/avutil.h>
#include <libavutil/hwcontext.h>
}

namespace {
// 自动释放FFmpeg资源，防止内存泄漏
struct AVFormatContextDeleter {
    void operator()(AVFormatContext* p) const { if (p) avformat_close_input(&p); }
};
struct AVCodecContextDeleter {
    void operator()(AVCodecContext* p) const { if (p) avcodec_free_context(&p); }
};
struct SwsContextDeleter {
    void operator()(SwsContext* p) const { if (p) sws_freeContext(p); }
};
struct AVFrameDeleter {
    void operator()(AVFrame* p) const { if (p) av_frame_free(&p); }
};
struct AVPacketDeleter {
    void operator()(AVPacket* p) const { if (p) av_packet_free(&p); }
};
struct AVBufferRefDeleter {
    void operator()(AVBufferRef* p) const { if (p) av_buffer_unref(&p); }
};
}

// 跨平台硬件设备初始化
static AVBufferRef *init_hardware_device()
{
    AVBufferRef *hw_device_ctx = nullptr;

#ifdef USE_D3D11VA
    // Windows系统：初始化D3D11VA硬件设备
    if (av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_D3D11VA,
                               nullptr, nullptr, 0) < 0)
    {
        return nullptr;
    }
#elif defined(USE_VAAPI)
    // Linux系统：初始化VA-API硬件设备
    if (av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_VAAPI,
                               "/dev/dri/renderD128", nullptr, 0) < 0)
    {
        return nullptr;
    }
#else
    // 未启用任何硬件加速,使用纯软件解码
    return nullptr;
#endif

    return hw_device_ctx;
}

// 跨平台硬件像素格式回调
static enum AVPixelFormat get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts)
{
    (void)ctx;
    const enum AVPixelFormat *p;

#ifdef USE_D3D11VA
    enum AVPixelFormat target_fmt = AV_PIX_FMT_D3D11;
#elif defined(USE_VAAPI)
    enum AVPixelFormat target_fmt = AV_PIX_FMT_VAAPI;
#else
    return pix_fmts[0];
#endif

    for (p = pix_fmts; *p != AV_PIX_FMT_NONE; p++)
    {
        if (*p == target_fmt)
            return target_fmt;
    }
    return pix_fmts[0];
}

Video::Video(QObject *parent)
    : QThread(parent), m_isPlaying(false)
{}

bool Video::open(const QString &url)
{
    m_url = url;
    return true;
}

void Video::stop()
{
    m_isPlaying = false;
    // 等待线程安全退出
    if (isRunning()) {
        wait(3000);
    }
}

void Video::run()
{
    m_isPlaying = true;
    bool hardwareDecodingEnabled = false;

    // 初始化硬件设备
    std::unique_ptr<AVBufferRef, AVBufferRefDeleter> hw_device_ctx(init_hardware_device());

    // 初始化网络（支持RTSP/HTTP流）
    avformat_network_init();
    struct NetworkDeinit {
        ~NetworkDeinit() { avformat_network_deinit(); }
    } network_deinit_guard;

    // 打开视频文件/流
    AVFormatContext *fmtCtx_raw = nullptr;
    if (avformat_open_input(&fmtCtx_raw, m_url.toUtf8().constData(), nullptr, nullptr) < 0)
    {
        return;
    }
    std::unique_ptr<AVFormatContext, AVFormatContextDeleter> fmtCtx(fmtCtx_raw);

    // 查找流信息
    if (avformat_find_stream_info(fmtCtx.get(), nullptr) < 0)
    {
        return;
    }

    // 找到视频流索引
    int videoIdx = -1;
    for (unsigned i = 0; i < fmtCtx->nb_streams; i++)
    {
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoIdx = i;
            break;
        }
    }
    if (videoIdx < 0)
    {
        return;
    }

    // 获取解码器
    AVCodecParameters *codecPar = fmtCtx->streams[videoIdx]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codecPar->codec_id);
    if (!codec)
    {
        return;
    }

    // 创建解码器上下文
    AVCodecContext *codecCtx_raw = avcodec_alloc_context3(codec);
    if (!codecCtx_raw) return;
    std::unique_ptr<AVCodecContext, AVCodecContextDeleter> codecCtx(codecCtx_raw);
    avcodec_parameters_to_context(codecCtx.get(), codecPar);

    // 尝试启用硬件解码
    if (hw_device_ctx)
    {
        codecCtx->hw_device_ctx = av_buffer_ref(hw_device_ctx.get());
        codecCtx->get_format = get_hw_format;
        if (avcodec_open2(codecCtx.get(), codec, nullptr) >= 0)
        {
            hardwareDecodingEnabled = true;
        }
        else
        {
            // 硬件解码失败，自动回退软件解码
            av_buffer_unref(&codecCtx->hw_device_ctx);
            codecCtx->get_format = nullptr;
            if (avcodec_open2(codecCtx.get(), codec, nullptr) < 0)
            {
                return;
            }
        }
    }
    else
    {
        if (avcodec_open2(codecCtx.get(), codec, nullptr) < 0)
        {
            return;
        }
    }

    // 分配帧和包内存
    std::unique_ptr<AVFrame, AVFrameDeleter> frame(av_frame_alloc());
    std::unique_ptr<AVFrame, AVFrameDeleter> sw_frame(av_frame_alloc()); // 硬件帧转CPU用
    std::unique_ptr<AVFrame, AVFrameDeleter> rgb_frame(av_frame_alloc()); // 格式转换用
    std::unique_ptr<AVPacket, AVPacketDeleter> pkt(av_packet_alloc());
    std::unique_ptr<SwsContext, SwsContextDeleter> swsCtx;

    if (!frame || !sw_frame || !rgb_frame || !pkt) return;

    int last_width = 0, last_height = 0;

    // 获取帧率
    AVRational frameRate = fmtCtx->streams[videoIdx]->avg_frame_rate;
    if (frameRate.den == 0 || frameRate.num == 0) frameRate = {30, 1};
    int frameDelay = 1000 * frameRate.den / frameRate.num;

    // 帧同步计时器
    QElapsedTimer timer;
    timer.start();
    int64_t firstPts = AV_NOPTS_VALUE;

    // 主解码循环
    while (m_isPlaying && av_read_frame(fmtCtx.get(), pkt.get()) == 0)
    {
        if (pkt->stream_index == videoIdx)
        {
            // 发送包到解码器
            int ret = avcodec_send_packet(codecCtx.get(), pkt.get());
            if (ret < 0)
                continue;

            // 接收解码后的帧
            while (avcodec_receive_frame(codecCtx.get(), frame.get()) == 0)
            {
                AVFrame *display_frame = frame.get();

                // 硬件帧下载到CPU内存
                if (hardwareDecodingEnabled)
                {
#ifdef USE_D3D11VA
                    if (frame->format == AV_PIX_FMT_D3D11)
#elif defined(USE_VAAPI)
                    if (frame->format == AV_PIX_FMT_VAAPI)
#endif
                    {
                        if (av_hwframe_transfer_data(sw_frame.get(), frame.get(), 0) < 0)
                        {
                            av_frame_unref(frame.get());
                            continue;
                        }
                        display_frame = sw_frame.get();
                    }
                }

                if (!swsCtx || display_frame->width != last_width || display_frame->height != last_height)
                {
                    swsCtx.reset(sws_getContext(
                        display_frame->width, display_frame->height, (enum AVPixelFormat)display_frame->format,
                        display_frame->width, display_frame->height, AV_PIX_FMT_RGB24,
                        SWS_FAST_BILINEAR, nullptr, nullptr, nullptr
                        ));
                    if (!swsCtx) {
                        av_frame_unref(frame.get());
                        continue;
                    }
                    last_width = display_frame->width;
                    last_height = display_frame->height;

                    // 分配RGB24帧缓冲区
                    if (rgb_frame->data[0]) {
                        av_freep(&rgb_frame->data[0]);
                    }
                    av_image_alloc(
                        rgb_frame->data, rgb_frame->linesize,
                        display_frame->width, display_frame->height,
                        AV_PIX_FMT_RGB24,
                        1
                        );
                }

                // 统一格式转换为RGB24
                sws_scale(
                    swsCtx.get(),
                    display_frame->data, display_frame->linesize,
                    0, display_frame->height,
                    rgb_frame->data, rgb_frame->linesize
                    );

                // 创建Qt 100%支持的RGB888格式图像
                QImage img(
                    rgb_frame->data[0],
                    display_frame->width,
                    display_frame->height,
                    rgb_frame->linesize[0],
                    QImage::Format_RGB888
                    );

                // 检查图像是否有效
                if (img.isNull()) {
                    av_frame_unref(frame.get());
                    continue;
                }

                // 发送帧到UI显示
                emit frameReady(img);

                // 精确帧同步
                if (frame->pts != AV_NOPTS_VALUE)
                {
                    if (firstPts == AV_NOPTS_VALUE)
                        firstPts = frame->pts;

                    int64_t shouldMs = av_rescale_q(
                        frame->pts - firstPts,
                        fmtCtx->streams[videoIdx]->time_base,
                        {1, 1000}
                        );

                    int waitMs = shouldMs - timer.elapsed();
                    if (waitMs > 0)
                        QThread::msleep(qMin(waitMs, frameDelay));
                }
                else
                {
                    QThread::msleep(frameDelay);
                }

                av_frame_unref(frame.get());
            }
        }
        av_packet_unref(pkt.get());
    }

    // 释放RGB帧缓冲区
    if (rgb_frame->data[0]) {
        av_freep(&rgb_frame->data[0]);
    }
}