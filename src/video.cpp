#include "video.h"
#include <QThread>
#include <QElapsedTimer>
#include <QImage>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/avutil.h>
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
}

void Video::run()
{
    m_isPlaying = true;

    //播 HTTP/HTTPS/RTSP
    avformat_network_init();

    //解封装上下文：存储视频容器信息（MP4/FLV等）
    AVFormatContext *fmtCtx = nullptr;

    //打开文件，失败直接退出
    if (avformat_open_input(&fmtCtx, m_url.toUtf8().constData(), nullptr, nullptr) < 0)
        return;
    //查找流信息
    if (avformat_find_stream_info(fmtCtx, nullptr) < 0)
        return;

    // 找到视频流的索引
    int videoIdx = -1;
    for (int i = 0; i < fmtCtx->nb_streams; i++)
    {
        // 判断流的类型是否为视频
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoIdx = i;
            break;
        }
    }
    //没找到视频流直接退出
    if (videoIdx < 0)
        return;

    //解码器
    //获取视频流的编码参数
    AVCodecParameters *codecPar = fmtCtx->streams[videoIdx]->codecpar;
    //根据编码ID（如 H.264）找到对应的解码器
    const AVCodec *codec = avcodec_find_decoder(codecPar->codec_id);
    //分配解码器
    AVCodecContext *codecCtx = avcodec_alloc_context3(codec);
    //拷贝编码参数
    avcodec_parameters_to_context(codecCtx, codecPar);
    //打开解码器
    avcodec_open2(codecCtx, codec, nullptr);

    //计算帧率，用于控制播放速度
    AVRational frameRate = fmtCtx->streams[videoIdx]->avg_frame_rate;
    //默认30帧
    if (frameRate.den == 0)
        frameRate = {30, 1};
    //帧间隔，毫秒
    int frameDelay = 1000 * frameRate.den / frameRate.num;

    //初始化图像格式转换器（SwsContext）FFmpeg 解码出来的原始数据一般是 YUV，但 Qt的QImage 只支持 RGB/BGR 格式
    // 用 SwsContext 把 YUV 转换成 BGR24
    SwsContext *swsCtx = sws_getContext(
        codecCtx->width, codecCtx->height, codecCtx->pix_fmt,
        codecCtx->width, codecCtx->height, AV_PIX_FMT_BGR24,
        SWS_BILINEAR, nullptr, nullptr, nullptr);

    //分配解码需要的数据包和帧
    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    AVFrame *rgbFrame = av_frame_alloc();
    //为 rgbFrame 分配内存缓冲区
    int bufSize = av_image_get_buffer_size(AV_PIX_FMT_BGR24, codecCtx->width, codecCtx->height, 1);
    uint8_t *buf = (uint8_t *)av_malloc(bufSize);
    av_image_fill_arrays(rgbFrame->data, rgbFrame->linesize, buf,
                         AV_PIX_FMT_BGR24, codecCtx->width, codecCtx->height, 1);

    //控制播放速度
    QElapsedTimer timer;
    timer.start();
    //第一帧的显示时间戳
    int64_t firstPts = AV_NOPTS_VALUE;

    //读包，解码，转换，发送给UI显示
    while (m_isPlaying && av_read_frame(fmtCtx, pkt) == 0)
    {
        if (pkt->stream_index == videoIdx)
        {
            avcodec_send_packet(codecCtx, pkt);
            while (avcodec_receive_frame(codecCtx, frame) == 0)
            {
                sws_scale(swsCtx, frame->data, frame->linesize, 0, codecCtx->height,
                          rgbFrame->data, rgbFrame->linesize);

                QImage img(rgbFrame->data[0], codecCtx->width, codecCtx->height, QImage::Format_BGR888);
                emit frameReady(img.copy());

                if (frame->pts != AV_NOPTS_VALUE)
                {
                    if (firstPts == AV_NOPTS_VALUE)
                        firstPts = frame->pts;
                    int64_t shouldMs = av_rescale_q(frame->pts - firstPts,
                                                    fmtCtx->streams[videoIdx]->time_base,
                                                    {1, 1000});
                    int waitMs = shouldMs - timer.elapsed();
                    if (waitMs > 0) QThread::msleep(qMin(waitMs, 100));
                }
                else
                {
                    QThread::msleep(frameDelay);
                }
            }
        }
        av_packet_unref(pkt);
    }

    //释放资源
    av_free(buf);
    av_frame_free(&frame);
    av_frame_free(&rgbFrame);
    av_packet_free(&pkt);
    sws_freeContext(swsCtx);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&fmtCtx);
    avformat_network_deinit();
}