#ifndef FFMPEGPLAYER_H
#define FFMPEGPLAYER_H

#include <QImage>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>

class AudioOutputController;

class FfmpegPlayer : public QThread
{
    Q_OBJECT

public:
    explicit FfmpegPlayer(QObject *parent = nullptr);
    ~FfmpegPlayer() override;

    void loadFile(const QString &filePath);
    void pausePlayback();
    void resumePlayback();
    void stopPlayback();
    bool isDecodingActive() const;

signals:
    void frameReady(const QImage &frame);
    void playbackStateChanged(bool isPlaying);
    void statusChanged(const QString &message);
    void errorOccurred(const QString &message);

protected:
    void run() override;

private:
    bool waitIfPausedOrStopped();
    void sleepWithControl(int milliseconds);
    void suspendAudioOutput();
    void resumeAudioOutput();
    qint64 processedAudioUsecs() const;

    mutable QMutex m_mutex;
    QWaitCondition m_pauseCondition;
    QString m_filePath;
    bool m_pauseRequested = false;
    bool m_stopRequested = false;
    AudioOutputController *m_audioOutput = nullptr;
};

#endif // FFMPEGPLAYER_H
