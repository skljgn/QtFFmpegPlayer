#ifndef WIDGETVIDEO_H
#define WIDGETVIDEO_H

#include <QWidget>

class FfmpegPlayer;
class QLabel;
class QPushButton;
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

private:
    void setupUi();
    void updateToggleButtonText();

    FfmpegPlayer *m_player = nullptr;
    VideoWidget *m_videoWidget = nullptr;
    QPushButton *m_openButton = nullptr;
    QPushButton *m_toggleButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    QString m_currentFile;
    bool m_isPlaying = false;
};

#endif // WIDGETVIDEO_H
