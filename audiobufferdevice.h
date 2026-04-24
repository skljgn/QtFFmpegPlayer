#ifndef AUDIOBUFFERDEVICE_H
#define AUDIOBUFFERDEVICE_H

#include <QByteArray>
#include <QIODevice>
#include <QMutex>

class AudioBufferDevice : public QIODevice
{
    Q_OBJECT

public:
    explicit AudioBufferDevice(QObject *parent = nullptr);

    void appendData(const QByteArray &data);
    void clearBuffer();
    qint64 bufferedBytes() const;

    qint64 bytesAvailable() const override;

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;

private:
    mutable QMutex m_mutex;
    QByteArray m_buffer;
};

#endif // AUDIOBUFFERDEVICE_H
