#include "audiobufferdevice.h"

#include <QMutexLocker>

#include <cstring>

AudioBufferDevice::AudioBufferDevice(QObject *parent)
    : QIODevice(parent)
{
    open(QIODevice::ReadOnly);
}

void AudioBufferDevice::appendData(const QByteArray &data)
{
    if (data.isEmpty()) {
        return;
    }

    {
        QMutexLocker locker(&m_mutex);
        m_buffer.append(data);
    }

    emit readyRead();
}

void AudioBufferDevice::clearBuffer()
{
    QMutexLocker locker(&m_mutex);
    m_buffer.clear();
}

qint64 AudioBufferDevice::bufferedBytes() const
{
    QMutexLocker locker(&m_mutex);
    return m_buffer.size();
}

qint64 AudioBufferDevice::bytesAvailable() const
{
    return bufferedBytes() + QIODevice::bytesAvailable();
}

qint64 AudioBufferDevice::readData(char *data, qint64 maxSize)
{
    if (data == nullptr || maxSize <= 0) {
        return 0;
    }

    QMutexLocker locker(&m_mutex);
    const qint64 bytesToRead = qMin(maxSize, static_cast<qint64>(m_buffer.size()));
    if (bytesToRead <= 0) {
        return 0;
    }

    std::memcpy(data, m_buffer.constData(), static_cast<size_t>(bytesToRead));
    m_buffer.remove(0, static_cast<qsizetype>(bytesToRead));
    return bytesToRead;
}

qint64 AudioBufferDevice::writeData(const char *data, qint64 maxSize)
{
    Q_UNUSED(data);
    Q_UNUSED(maxSize);
    return -1;
}
