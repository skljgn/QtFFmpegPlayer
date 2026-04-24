#include "audiooutputcontroller.h"

#include "audiobufferdevice.h"

#include <QAudio>
#include <QAudioDevice>
#include <QAudioSink>
#include <QMediaDevices>

AudioOutputController::AudioOutputController(QObject *parent)
    : QObject(parent)
    , m_bufferDevice(new AudioBufferDevice(this))
{
}

AudioOutputController::~AudioOutputController()
{
    stopOutput();
}

QAudioFormat AudioOutputController::defaultOutputFormat() const
{
    const QAudioDevice deviceInfo = QMediaDevices::defaultAudioOutput();
    return deviceInfo.isNull() ? QAudioFormat() : deviceInfo.preferredFormat();
}

QString AudioOutputController::startOutput(const QAudioFormat &format)
{
    stopOutput();
    m_bufferDevice->clearBuffer();

    const QAudioDevice deviceInfo = QMediaDevices::defaultAudioOutput();
    if (deviceInfo.isNull()) {
        return tr("未找到可用的音频输出设备");
    }

    m_audioSink = new QAudioSink(deviceInfo, format, this);
    m_audioSink->start(m_bufferDevice);

    if (m_audioSink->state() == QAudio::StoppedState && m_audioSink->error() != QtAudio::NoError) {
        const QString errorMessage = tr("启动音频输出失败");
        stopOutput();
        return errorMessage;
    }

    return QString();
}

void AudioOutputController::stopOutput()
{
    if (m_audioSink == nullptr) {
        return;
    }

    m_audioSink->stop();
    delete m_audioSink;
    m_audioSink = nullptr;
    m_bufferDevice->clearBuffer();
}

void AudioOutputController::suspendOutput()
{
    if (m_audioSink != nullptr) {
        m_audioSink->suspend();
    }
}

void AudioOutputController::resumeOutput()
{
    if (m_audioSink != nullptr) {
        m_audioSink->resume();
    }
}

qint64 AudioOutputController::processedUSecs() const
{
    return m_audioSink == nullptr ? 0 : m_audioSink->processedUSecs();
}

void AudioOutputController::appendAudioData(const QByteArray &data)
{
    m_bufferDevice->appendData(data);
}

void AudioOutputController::clearBuffer()
{
    m_bufferDevice->clearBuffer();
}

qint64 AudioOutputController::bufferedBytes() const
{
    return m_bufferDevice->bufferedBytes();
}
