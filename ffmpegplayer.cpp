#include "ffmpegplayer.h"

#include "audiooutputcontroller.h"

#include <QAudioFormat>
#include <QByteArray>
#include <QFileInfo>
#include <QMetaObject>
#include <QMutexLocker>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

namespace {

QString ffmpegErrorToString(int errorCode)
{
    char errorBuffer[AV_ERROR_MAX_STRING_SIZE] = {};
    av_make_error_string(errorBuffer, sizeof(errorBuffer), errorCode);
    return QString::fromUtf8(errorBuffer);
}

int calculateFrameDelayMs(const AVStream *stream, const AVFrame *frame, double &lastPtsSeconds)
{
    double delaySeconds = 0.0;

    if (frame->best_effort_timestamp != AV_NOPTS_VALUE) {
        const double currentPtsSeconds =
            frame->best_effort_timestamp * av_q2d(stream->time_base);

        if (lastPtsSeconds >= 0.0) {
            delaySeconds = currentPtsSeconds - lastPtsSeconds;
        }

        lastPtsSeconds = currentPtsSeconds;
    }

    if (delaySeconds <= 0.0 || delaySeconds > 1.0) {
        if (stream->avg_frame_rate.num > 0 && stream->avg_frame_rate.den > 0) {
            delaySeconds = av_q2d(av_inv_q(stream->avg_frame_rate));
        } else if (stream->r_frame_rate.num > 0 && stream->r_frame_rate.den > 0) {
            delaySeconds = av_q2d(av_inv_q(stream->r_frame_rate));
        } else {
            delaySeconds = 1.0 / 25.0;
        }
    }

    return qMax(1, static_cast<int>(delaySeconds * 1000.0));
}

AVSampleFormat toAvSampleFormat(QAudioFormat::SampleFormat sampleFormat)
{
    switch (sampleFormat) {
    case QAudioFormat::UInt8:
        return AV_SAMPLE_FMT_U8;
    case QAudioFormat::Int16:
        return AV_SAMPLE_FMT_S16;
    case QAudioFormat::Int32:
        return AV_SAMPLE_FMT_S32;
    case QAudioFormat::Float:
        return AV_SAMPLE_FMT_FLT;
    case QAudioFormat::Unknown:
    case QAudioFormat::NSampleFormats:
        break;
    }

    return AV_SAMPLE_FMT_NONE;
}

QString formatPlaybackName(const QString &prefix, const QString &filePath)
{
    return QObject::tr("%1: %2").arg(prefix, QFileInfo(filePath).fileName());
}

} // namespace

FfmpegPlayer::FfmpegPlayer(QObject *parent)
    : QThread(parent)
    , m_audioOutput(new AudioOutputController(this))
{
}

FfmpegPlayer::~FfmpegPlayer()
{
    stopPlayback();
}

void FfmpegPlayer::loadFile(const QString &filePath)
{
    stopPlayback();

    {
        QMutexLocker locker(&m_mutex);
        m_filePath = filePath;
        m_pauseRequested = false;
        m_stopRequested = false;
    }

    start();
}

void FfmpegPlayer::pausePlayback()
{
    QString filePath;
    {
        QMutexLocker locker(&m_mutex);
        m_pauseRequested = true;
        filePath = m_filePath;
    }

    emit playbackStateChanged(false);
    emit statusChanged(formatPlaybackName(tr("已暂停"), filePath));
}

void FfmpegPlayer::resumePlayback()
{
    QString filePath;
    {
        QMutexLocker locker(&m_mutex);
        m_pauseRequested = false;
        m_pauseCondition.wakeAll();
        filePath = m_filePath;
    }

    emit playbackStateChanged(true);
    emit statusChanged(formatPlaybackName(tr("正在播放"), filePath));
}

void FfmpegPlayer::stopPlayback()
{
    {
        QMutexLocker locker(&m_mutex);
        m_stopRequested = true;
        m_pauseRequested = false;
        m_pauseCondition.wakeAll();
    }

    wait();
}

bool FfmpegPlayer::isDecodingActive() const
{
    return isRunning();
}

void FfmpegPlayer::run()
{
    QString filePath;
    {
        QMutexLocker locker(&m_mutex);
        filePath = m_filePath;
    }

    if (filePath.isEmpty()) {
        emit playbackStateChanged(false);
        return;
    }

    AVFormatContext *formatContext = nullptr;
    AVCodecContext *videoCodecContext = nullptr;
    AVCodecContext *audioCodecContext = nullptr;
    const AVCodec *videoCodec = nullptr;
    const AVCodec *audioCodec = nullptr;
    AVPacket *packet = nullptr;
    AVFrame *videoFrame = nullptr;
    AVFrame *rgbFrame = nullptr;
    AVFrame *audioFrame = nullptr;
    SwsContext *swsContext = nullptr;
    SwrContext *swrContext = nullptr;
    uint8_t *videoBuffer = nullptr;
    AVChannelLayout outputChannelLayout = {};
    bool outputChannelLayoutInitialized = false;
    int videoStreamIndex = -1;
    int audioStreamIndex = -1;
    int imageBufferSize = 0;
    int rgbWidth = 0;
    int rgbHeight = 0;
    double lastPtsSeconds = -1.0;
    qint64 firstVideoPtsMs = -1;
    bool firstVideoFrameEmitted = false;
    AVSampleFormat outputSampleFormat = AV_SAMPLE_FMT_NONE;
    QAudioFormat outputAudioFormat;
    qint64 audioBufferLimitBytes = 0;
    bool audioOutputStarted = false;

    const auto cleanup = [&]() {
        if (audioOutputStarted) {
            QMetaObject::invokeMethod(m_audioOutput, [this]() {
                m_audioOutput->stopOutput();
            }, Qt::BlockingQueuedConnection);
            audioOutputStarted = false;
        }
        if (outputChannelLayoutInitialized) {
            av_channel_layout_uninit(&outputChannelLayout);
        }
        if (videoBuffer != nullptr) {
            av_free(videoBuffer);
        }
        if (swrContext != nullptr) {
            swr_free(&swrContext);
        }
        if (swsContext != nullptr) {
            sws_freeContext(swsContext);
        }
        if (rgbFrame != nullptr) {
            av_frame_free(&rgbFrame);
        }
        if (audioFrame != nullptr) {
            av_frame_free(&audioFrame);
        }
        if (videoFrame != nullptr) {
            av_frame_free(&videoFrame);
        }
        if (packet != nullptr) {
            av_packet_free(&packet);
        }
        if (audioCodecContext != nullptr) {
            avcodec_free_context(&audioCodecContext);
        }
        if (videoCodecContext != nullptr) {
            avcodec_free_context(&videoCodecContext);
        }
        if (formatContext != nullptr) {
            avformat_close_input(&formatContext);
        }
    };

    const auto fail = [&](const QString &message) {
        emit playbackStateChanged(false);
        emit errorOccurred(message);
        cleanup();
    };

    const auto writeAudioBuffer = [&](const char *data, int size) -> bool {
        if (!audioOutputStarted || data == nullptr || size <= 0) {
            return true;
        }

        const QByteArray audioBytes(data, size);
        while (m_audioOutput->bufferedBytes() > audioBufferLimitBytes) {
            if (!waitIfPausedOrStopped()) {
                return false;
            }
            sleepWithControl(5);
        }

        m_audioOutput->appendAudioData(audioBytes);
        return true;
    };

    int result = avformat_open_input(&formatContext, filePath.toUtf8().constData(), nullptr, nullptr);
    if (result < 0) {
        fail(tr("打开文件失败: %1").arg(ffmpegErrorToString(result)));
        return;
    }

    result = avformat_find_stream_info(formatContext, nullptr);
    if (result < 0) {
        fail(tr("读取流信息失败: %1").arg(ffmpegErrorToString(result)));
        return;
    }

    result = av_find_best_stream(formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, &videoCodec, 0);
    if (result < 0) {
        fail(tr("没有找到可用的视频流: %1").arg(ffmpegErrorToString(result)));
        return;
    }
    videoStreamIndex = result;

    videoCodecContext = avcodec_alloc_context3(videoCodec);
    if (videoCodecContext == nullptr) {
        fail(tr("创建视频解码器上下文失败"));
        return;
    }

    result = avcodec_parameters_to_context(videoCodecContext,
                                           formatContext->streams[videoStreamIndex]->codecpar);
    if (result < 0) {
        fail(tr("复制视频解码参数失败: %1").arg(ffmpegErrorToString(result)));
        return;
    }

    result = avcodec_open2(videoCodecContext, videoCodec, nullptr);
    if (result < 0) {
        fail(tr("打开视频解码器失败: %1").arg(ffmpegErrorToString(result)));
        return;
    }

    result = av_find_best_stream(formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, &audioCodec, 0);
    if (result >= 0) {
        audioStreamIndex = result;

        audioCodecContext = avcodec_alloc_context3(audioCodec);
        if (audioCodecContext == nullptr) {
            fail(tr("创建音频解码器上下文失败"));
            return;
        }

        result = avcodec_parameters_to_context(audioCodecContext,
                                               formatContext->streams[audioStreamIndex]->codecpar);
        if (result < 0) {
            fail(tr("复制音频解码参数失败: %1").arg(ffmpegErrorToString(result)));
            return;
        }

        result = avcodec_open2(audioCodecContext, audioCodec, nullptr);
        if (result < 0) {
            fail(tr("打开音频解码器失败: %1").arg(ffmpegErrorToString(result)));
            return;
        }

        QMetaObject::invokeMethod(m_audioOutput, [this, &outputAudioFormat]() {
            outputAudioFormat = m_audioOutput->defaultOutputFormat();
        }, Qt::BlockingQueuedConnection);

        if (!outputAudioFormat.isValid()) {
            fail(tr("未找到可用的音频输出设备"));
            return;
        }
        if (outputAudioFormat.sampleRate() <= 0) {
            outputAudioFormat.setSampleRate(qMax(1, audioCodecContext->sample_rate));
        }
        if (outputAudioFormat.channelCount() <= 0) {
            outputAudioFormat.setChannelCount(qMax(1, audioCodecContext->ch_layout.nb_channels));
        }
        if (outputAudioFormat.sampleFormat() == QAudioFormat::Unknown) {
            outputAudioFormat.setSampleFormat(QAudioFormat::Int16);
        }

        outputSampleFormat = toAvSampleFormat(outputAudioFormat.sampleFormat());
        if (outputSampleFormat == AV_SAMPLE_FMT_NONE) {
            fail(tr("当前音频设备的采样格式暂不支持"));
            return;
        }

        av_channel_layout_default(&outputChannelLayout, outputAudioFormat.channelCount());
        outputChannelLayoutInitialized = true;

        result = swr_alloc_set_opts2(&swrContext,
                                     &outputChannelLayout,
                                     outputSampleFormat,
                                     outputAudioFormat.sampleRate(),
                                     &audioCodecContext->ch_layout,
                                     audioCodecContext->sample_fmt,
                                     audioCodecContext->sample_rate,
                                     0,
                                     nullptr);
        if (result < 0 || swrContext == nullptr) {
            fail(tr("创建音频重采样器失败: %1").arg(ffmpegErrorToString(result)));
            return;
        }

        result = swr_init(swrContext);
        if (result < 0) {
            fail(tr("初始化音频重采样器失败: %1").arg(ffmpegErrorToString(result)));
            return;
        }

        QString audioStartError;
        QMetaObject::invokeMethod(m_audioOutput, [this, &outputAudioFormat, &audioStartError]() {
            audioStartError = m_audioOutput->startOutput(outputAudioFormat);
        }, Qt::BlockingQueuedConnection);
        if (!audioStartError.isEmpty()) {
            fail(audioStartError);
            return;
        }

        audioOutputStarted = true;
        audioBufferLimitBytes = outputAudioFormat.bytesForDuration(200000);
        if (audioBufferLimitBytes <= 0) {
            audioBufferLimitBytes = 32768;
        }
    }

    packet = av_packet_alloc();
    videoFrame = av_frame_alloc();
    rgbFrame = av_frame_alloc();
    audioFrame = av_frame_alloc();

    if (packet == nullptr || videoFrame == nullptr || rgbFrame == nullptr || audioFrame == nullptr) {
        fail(tr("分配 FFmpeg 缓冲区失败"));
        return;
    }

    emit playbackStateChanged(true);
    emit statusChanged(formatPlaybackName(tr("正在播放"), filePath));

    AVStream *videoStream = formatContext->streams[videoStreamIndex];

    while (true) {
        if (!waitIfPausedOrStopped()) {
            break;
        }

        result = av_read_frame(formatContext, packet);
        if (result < 0) {
            break;
        }

        if (packet->stream_index == videoStreamIndex) {
            result = avcodec_send_packet(videoCodecContext, packet);
            av_packet_unref(packet);

            if (result < 0) {
                fail(tr("发送视频包到解码器失败: %1").arg(ffmpegErrorToString(result)));
                return;
            }

            while (result >= 0) {
                if (!waitIfPausedOrStopped()) {
                    cleanup();
                    emit playbackStateChanged(false);
                    emit statusChanged(tr("已停止"));
                    return;
                }

                result = avcodec_receive_frame(videoCodecContext, videoFrame);
                if (result == AVERROR(EAGAIN) || result == AVERROR_EOF) {
                    break;
                }

                if (result < 0) {
                    fail(tr("读取视频帧失败: %1").arg(ffmpegErrorToString(result)));
                    return;
                }

                if (videoFrame->width <= 0 || videoFrame->height <= 0 ||
                    videoFrame->format == AV_PIX_FMT_NONE) {
                    fail(tr("视频帧信息无效，无法初始化图像缓冲区"));
                    return;
                }

                if (swsContext == nullptr ||
                    rgbWidth != videoFrame->width ||
                    rgbHeight != videoFrame->height) {
                    if (videoBuffer != nullptr) {
                        av_free(videoBuffer);
                        videoBuffer = nullptr;
                    }
                    if (swsContext != nullptr) {
                        sws_freeContext(swsContext);
                        swsContext = nullptr;
                    }

                    rgbWidth = videoFrame->width;
                    rgbHeight = videoFrame->height;

                    imageBufferSize = av_image_get_buffer_size(AV_PIX_FMT_RGB24,
                                                               rgbWidth,
                                                               rgbHeight,
                                                               1);
                    if (imageBufferSize < 0) {
                        fail(tr("计算图像缓冲区大小失败: %1")
                                 .arg(ffmpegErrorToString(imageBufferSize)));
                        return;
                    }

                    videoBuffer = static_cast<uint8_t *>(av_malloc(imageBufferSize));
                    if (videoBuffer == nullptr) {
                        fail(tr("分配图像缓冲区失败"));
                        return;
                    }

                    result = av_image_fill_arrays(rgbFrame->data,
                                                  rgbFrame->linesize,
                                                  videoBuffer,
                                                  AV_PIX_FMT_RGB24,
                                                  rgbWidth,
                                                  rgbHeight,
                                                  1);
                    if (result < 0) {
                        fail(tr("初始化 RGB 帧失败: %1").arg(ffmpegErrorToString(result)));
                        return;
                    }

                    swsContext = sws_getContext(videoFrame->width,
                                                videoFrame->height,
                                                static_cast<AVPixelFormat>(videoFrame->format),
                                                rgbWidth,
                                                rgbHeight,
                                                AV_PIX_FMT_RGB24,
                                                SWS_BILINEAR,
                                                nullptr,
                                                nullptr,
                                                nullptr);
                    if (swsContext == nullptr) {
                        fail(tr("创建像素格式转换器失败"));
                        return;
                    }
                }

                sws_scale(swsContext,
                          videoFrame->data,
                          videoFrame->linesize,
                          0,
                          videoFrame->height,
                          rgbFrame->data,
                          rgbFrame->linesize);

                const QImage frameImage(rgbFrame->data[0],
                                        rgbWidth,
                                        rgbHeight,
                                        rgbFrame->linesize[0],
                                        QImage::Format_RGB888);
                emit frameReady(frameImage.copy());
                if (!firstVideoFrameEmitted) {
                    firstVideoFrameEmitted = true;
                    emit statusChanged(tr("已解码首帧视频"));
                }

                if (audioOutputStarted &&
                    videoFrame->best_effort_timestamp != AV_NOPTS_VALUE) {
                    const qint64 rawFramePtsMs = static_cast<qint64>(
                        videoFrame->best_effort_timestamp * av_q2d(videoStream->time_base) * 1000.0);
                    if (firstVideoPtsMs < 0) {
                        firstVideoPtsMs = rawFramePtsMs;
                    }

                    const qint64 relativeFramePtsMs = rawFramePtsMs - firstVideoPtsMs;
                    const qint64 audioClockMs = processedAudioUsecs() / 1000;
                    const int waitMs = static_cast<int>(relativeFramePtsMs - audioClockMs);
                    if (waitMs > 1) {
                        sleepWithControl(waitMs);
                    }
                } else {
                    sleepWithControl(calculateFrameDelayMs(videoStream, videoFrame, lastPtsSeconds));
                }
            }
        } else if (packet->stream_index == audioStreamIndex &&
                   audioCodecContext != nullptr &&
                   swrContext != nullptr &&
                   audioOutputStarted) {
            result = avcodec_send_packet(audioCodecContext, packet);
            av_packet_unref(packet);

            if (result < 0) {
                fail(tr("发送音频包到解码器失败: %1").arg(ffmpegErrorToString(result)));
                return;
            }

            while (result >= 0) {
                if (!waitIfPausedOrStopped()) {
                    cleanup();
                    emit playbackStateChanged(false);
                    emit statusChanged(tr("已停止"));
                    return;
                }

                result = avcodec_receive_frame(audioCodecContext, audioFrame);
                if (result == AVERROR(EAGAIN) || result == AVERROR_EOF) {
                    break;
                }

                if (result < 0) {
                    fail(tr("读取音频帧失败: %1").arg(ffmpegErrorToString(result)));
                    return;
                }

                const int outSamples = av_rescale_rnd(
                    swr_get_delay(swrContext, audioCodecContext->sample_rate) + audioFrame->nb_samples,
                    outputAudioFormat.sampleRate(),
                    audioCodecContext->sample_rate,
                    AV_ROUND_UP);

                const int bufferSize = av_samples_get_buffer_size(nullptr,
                                                                  outputAudioFormat.channelCount(),
                                                                  outSamples,
                                                                  outputSampleFormat,
                                                                  1);
                if (bufferSize < 0) {
                    fail(tr("计算音频缓冲区大小失败: %1").arg(ffmpegErrorToString(bufferSize)));
                    return;
                }

                QByteArray audioBuffer(bufferSize, 0);
                uint8_t *outputData[1] = {
                    reinterpret_cast<uint8_t *>(audioBuffer.data())
                };

                const int convertedSamples = swr_convert(swrContext,
                                                         outputData,
                                                         outSamples,
                                                         const_cast<const uint8_t **>(audioFrame->extended_data),
                                                         audioFrame->nb_samples);
                if (convertedSamples < 0) {
                    fail(tr("音频重采样失败: %1").arg(ffmpegErrorToString(convertedSamples)));
                    return;
                }

                const int convertedSize = av_samples_get_buffer_size(nullptr,
                                                                     outputAudioFormat.channelCount(),
                                                                     convertedSamples,
                                                                     outputSampleFormat,
                                                                     1);
                if (convertedSize < 0) {
                    fail(tr("计算音频输出大小失败: %1")
                             .arg(ffmpegErrorToString(convertedSize)));
                    return;
                }

                if (!writeAudioBuffer(audioBuffer.constData(), convertedSize)) {
                    cleanup();
                    emit playbackStateChanged(false);
                    emit statusChanged(tr("已停止"));
                    return;
                }
            }
        } else {
            av_packet_unref(packet);
        }
    }

    cleanup();
    emit playbackStateChanged(false);
    emit statusChanged(formatPlaybackName(tr("播放完成"), filePath));
}

bool FfmpegPlayer::waitIfPausedOrStopped()
{
    bool wasPaused = false;
    {
        QMutexLocker locker(&m_mutex);
        if (m_stopRequested) {
            return false;
        }
        wasPaused = m_pauseRequested;
    }

    if (wasPaused) {
        suspendAudioOutput();
    }

    bool shouldContinue = true;
    {
        QMutexLocker locker(&m_mutex);
        while (!m_stopRequested && m_pauseRequested) {
            m_pauseCondition.wait(&m_mutex);
        }
        shouldContinue = !m_stopRequested;
    }

    if (wasPaused && shouldContinue) {
        resumeAudioOutput();
    }

    return shouldContinue;
}

void FfmpegPlayer::sleepWithControl(int milliseconds)
{
    int remaining = milliseconds;
    while (remaining > 0) {
        bool wasPaused = false;
        bool shouldContinue = true;
        {
            QMutexLocker locker(&m_mutex);
            if (m_stopRequested) {
                return;
            }

            wasPaused = m_pauseRequested;
        }

        if (wasPaused) {
            suspendAudioOutput();
        }

        {
            QMutexLocker locker(&m_mutex);
            while (!m_stopRequested && m_pauseRequested) {
                m_pauseCondition.wait(&m_mutex);
            }
            shouldContinue = !m_stopRequested;
        }

        if (wasPaused && shouldContinue) {
            resumeAudioOutput();
        }

        if (!shouldContinue) {
            return;
        }

        const int chunk = qMin(remaining, 10);
        msleep(static_cast<unsigned long>(chunk));
        remaining -= chunk;
    }
}

void FfmpegPlayer::suspendAudioOutput()
{
    QMetaObject::invokeMethod(m_audioOutput, [this]() {
        m_audioOutput->suspendOutput();
    }, Qt::BlockingQueuedConnection);
}

void FfmpegPlayer::resumeAudioOutput()
{
    QMetaObject::invokeMethod(m_audioOutput, [this]() {
        m_audioOutput->resumeOutput();
    }, Qt::BlockingQueuedConnection);
}

qint64 FfmpegPlayer::processedAudioUsecs() const
{
    qint64 processedUsecs = 0;
    QMetaObject::invokeMethod(m_audioOutput, [this, &processedUsecs]() {
        processedUsecs = m_audioOutput->processedUSecs();
    }, Qt::BlockingQueuedConnection);
    return processedUsecs;
}
