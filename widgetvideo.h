#ifndef WIDGETVIDEO_H
#define WIDGETVIDEO_H

#include <QWidget>

class FfmpegPlayer;
class QLabel;
class QPushButton;
class QSlider;
class VideoWidget;

class WidgetVideo : public QWidget
{
    Q_OBJECT

public:
    explicit WidgetVideo(QWidget *parent = nullptr);
    ~WidgetVideo() override;

private slots:
    void openVideo();
    void togglePlayback();
    void updatePlaybackState(bool isPlaying);
    void showStatusMessage(const QString &message);
    void showErrorMessage(const QString &message);
    void updateDuration(qint64 durationMs);
    void updatePosition(qint64 positionMs);
    void handleSliderPressed();
    void handleSliderReleased();
    void handleSliderMoved(int value);

private:
    void setupUi();
    void updateToggleButtonText();
    void resetProgress();
    QString formatTime(qint64 positionMs) const;

    FfmpegPlayer *m_player = nullptr;
    VideoWidget *m_videoWidget = nullptr;
    QPushButton *m_openButton = nullptr;
    QPushButton *m_toggleButton = nullptr;
    QSlider *m_progressSlider = nullptr;
    QLabel *m_currentTimeLabel = nullptr;
    QLabel *m_durationLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QString m_currentFile;
    bool m_isPlaying = false;
    bool m_isSliderPressed = false;
    qint64 m_durationMs = 0;
};

#endif // WIDGETVIDEO_H
