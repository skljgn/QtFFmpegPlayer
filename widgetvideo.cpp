#include "widgetvideo.h"

#include "ffmpegplayer.h"
#include "videowidget.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

#include <climits>

WidgetVideo::WidgetVideo(QWidget *parent)
    : QWidget(parent)
    , m_player(new FfmpegPlayer(this))
{
    setupUi();

    connect(m_openButton, &QPushButton::clicked, this, &WidgetVideo::openVideo);
    connect(m_toggleButton, &QPushButton::clicked, this, &WidgetVideo::togglePlayback);
    connect(m_player, &FfmpegPlayer::frameReady, m_videoWidget, &VideoWidget::setFrame);
    connect(m_player, &FfmpegPlayer::playbackStateChanged,
            this, &WidgetVideo::updatePlaybackState);
    connect(m_player, &FfmpegPlayer::statusChanged,
            this, &WidgetVideo::showStatusMessage);
    connect(m_player, &FfmpegPlayer::errorOccurred,
            this, &WidgetVideo::showErrorMessage);
    connect(m_player, &FfmpegPlayer::durationChanged,
            this, &WidgetVideo::updateDuration);
    connect(m_player, &FfmpegPlayer::positionChanged,
            this, &WidgetVideo::updatePosition);
    connect(m_progressSlider, &QSlider::sliderPressed,
            this, &WidgetVideo::handleSliderPressed);
    connect(m_progressSlider, &QSlider::sliderReleased,
            this, &WidgetVideo::handleSliderReleased);
    connect(m_progressSlider, &QSlider::sliderMoved,
            this, &WidgetVideo::handleSliderMoved);

    updateToggleButtonText();
    resetProgress();
}

WidgetVideo::~WidgetVideo()
{
    m_player->stopPlayback();
}

void WidgetVideo::openVideo()
{
    const QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("Open Video"),
        QString(),
        tr("Common Video Files (*.mp4 *.mov *.m4v *.mkv *.avi *.flv *.wmv *.ts *.mts *.m2ts *.webm *.mpg *.mpeg *.3gp);;All Files (*)"));

    if (fileName.isEmpty()) {
        return;
    }

    m_currentFile = fileName;
    m_videoWidget->clearFrame();
    resetProgress();
    m_player->loadFile(fileName);
}

void WidgetVideo::togglePlayback()
{
    if (m_currentFile.isEmpty()) {
        openVideo();
        return;
    }

    if (!m_player->isDecodingActive()) {
        m_videoWidget->clearFrame();
        m_player->loadFile(m_currentFile);
        return;
    }

    if (m_isPlaying) {
        m_player->pausePlayback();
    } else {
        m_player->resumePlayback();
    }
}

void WidgetVideo::updatePlaybackState(bool isPlaying)
{
    m_isPlaying = isPlaying;
    updateToggleButtonText();
}

void WidgetVideo::showStatusMessage(const QString &message)
{
    m_statusLabel->setText(message);
}

void WidgetVideo::showErrorMessage(const QString &message)
{
    m_statusLabel->setText(tr("播放失败"));
    QMessageBox::warning(this, tr("FFmpeg 播放失败"), message);
}

void WidgetVideo::updateDuration(qint64 durationMs)
{
    m_durationMs = qMax<qint64>(0, durationMs);
    m_progressSlider->setEnabled(m_durationMs > 0);
    m_progressSlider->setRange(0, static_cast<int>(qMin<qint64>(m_durationMs, INT_MAX)));
    m_durationLabel->setText(formatTime(m_durationMs));

    if (!m_isSliderPressed) {
        m_progressSlider->setValue(0);
        m_currentTimeLabel->setText(formatTime(0));
    }
}

void WidgetVideo::updatePosition(qint64 positionMs)
{
    const qint64 safePositionMs = qMax<qint64>(0, positionMs);
    if (m_isSliderPressed) {
        return;
    }

    m_progressSlider->setValue(static_cast<int>(qMin<qint64>(safePositionMs, INT_MAX)));
    m_currentTimeLabel->setText(formatTime(safePositionMs));
}

void WidgetVideo::handleSliderPressed()
{
    m_isSliderPressed = true;
}

void WidgetVideo::handleSliderReleased()
{
    m_isSliderPressed = false;
    const qint64 targetPositionMs = m_progressSlider->value();
    m_currentTimeLabel->setText(formatTime(targetPositionMs));
    m_player->seekTo(targetPositionMs);
}

void WidgetVideo::handleSliderMoved(int value)
{
    if (!m_isSliderPressed) {
        return;
    }

    m_currentTimeLabel->setText(formatTime(value));
}

void WidgetVideo::setupUi()
{
    resize(960, 640);
    setWindowTitle(tr("FFmpeg 本地 MP4 播放器"));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    m_videoWidget = new VideoWidget(this);
    m_videoWidget->setMinimumSize(640, 360);
    mainLayout->addWidget(m_videoWidget, 1);

    auto *progressLayout = new QHBoxLayout();
    progressLayout->setSpacing(8);

    m_currentTimeLabel = new QLabel(tr("00:00"), this);
    m_durationLabel = new QLabel(tr("00:00"), this);
    m_progressSlider = new QSlider(Qt::Horizontal, this);
    m_progressSlider->setEnabled(false);

    progressLayout->addWidget(m_currentTimeLabel);
    progressLayout->addWidget(m_progressSlider, 1);
    progressLayout->addWidget(m_durationLabel);

    mainLayout->addLayout(progressLayout);

    auto *controlLayout = new QHBoxLayout();
    controlLayout->setSpacing(8);

    m_openButton = new QPushButton(tr("打开 MP4"), this);
    m_toggleButton = new QPushButton(this);
    m_statusLabel = new QLabel(tr("请选择一个本地 MP4 文件"), this);
    m_statusLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    controlLayout->addWidget(m_openButton);
    controlLayout->addWidget(m_toggleButton);
    controlLayout->addWidget(m_statusLabel, 1);

    mainLayout->addLayout(controlLayout);
}

void WidgetVideo::updateToggleButtonText()
{
    if (m_currentFile.isEmpty()) {
        m_toggleButton->setText(tr("打开并播放"));
        return;
    }

    m_toggleButton->setText(m_isPlaying ? tr("暂停") : tr("播放"));
}

void WidgetVideo::resetProgress()
{
    m_durationMs = 0;
    m_isSliderPressed = false;
    m_progressSlider->setEnabled(false);
    m_progressSlider->setRange(0, 0);
    m_progressSlider->setValue(0);
    m_currentTimeLabel->setText(formatTime(0));
    m_durationLabel->setText(formatTime(0));
}

QString WidgetVideo::formatTime(qint64 positionMs) const
{
    const qint64 totalSeconds = qMax<qint64>(0, positionMs) / 1000;
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;

    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours, 2, 10, QLatin1Char('0'))
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0'));
    }

    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}
