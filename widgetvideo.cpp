#include "widgetvideo.h"

#include "ffmpegplayer.h"
#include "videowidget.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

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

    updateToggleButtonText();
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
