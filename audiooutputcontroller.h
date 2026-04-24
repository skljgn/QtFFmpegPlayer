#ifndef AUDIOOUTPUTCONTROLLER_H
#define AUDIOOUTPUTCONTROLLER_H

#include <QAudioFormat>
#include <QObject>

class AudioBufferDevice;
class QAudioSink;

class AudioOutputController : public QObject
{
    Q_OBJECT

public:
    explicit AudioOutputController(QObject *parent = nullptr);
    ~AudioOutputController() override;

    QAudioFormat defaultOutputFormat() const;
    QString startOutput(const QAudioFormat &format);
    void stopOutput();
    void suspendOutput();
    void resumeOutput();
    qint64 processedUSecs() const;

    void appendAudioData(const QByteArray &data);
    void clearBuffer();
    qint64 bufferedBytes() const;

private:
    AudioBufferDevice *m_bufferDevice = nullptr;
    QAudioSink *m_audioSink = nullptr;
};

#endif // AUDIOOUTPUTCONTROLLER_H
