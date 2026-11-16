#include "widgets/splits/StreamPlayer.hpp"

#include "providers/twitch/ViewerPresence.hpp"

#include <QAudioOutput>
#include <QBoxLayout>
#include <QMediaPlayer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPushButton>
#include <QRegularExpression>
#include <QVideoWidget>

namespace chatterino {

namespace {

// pick the variant closest to (but not above) 480p; skip audio-only
QString pickVariant(const QString &master)
{
    static const QRegularExpression res("RESOLUTION=\\d+x(\\d+)");
    int bestH = -1;
    int h = 0;
    QString best;
    for (const auto &line : master.split('\n'))
    {
        auto l = line.trimmed();
        if (l.startsWith("#EXT-X-STREAM-INF"))
        {
            auto m = res.match(l);
            h = m.hasMatch() ? m.captured(1).toInt() : 0;
        }
        else if (!l.isEmpty() && !l.startsWith('#') && h > 0)
        {
            bool better = bestH < 0 || (h <= 480 && (bestH > 480 || h > bestH)) ||
                          (h > 480 && bestH > 480 && h < bestH);
            if (better)
            {
                bestH = h;
                best = l;
            }
        }
    }
    return best;
}

}  // namespace

StreamPlayer::StreamPlayer(QWidget *parent)
    : QWidget(parent)
    , player_(new QMediaPlayer(this))
    , audio_(new QAudioOutput(this))
    , video_(new QVideoWidget(this))
    , muteButton_(new QPushButton(this))
    , bar_(new QWidget(this))
    , nam_(new QNetworkAccessManager(this))
{
    this->setAttribute(Qt::WA_StyledBackground);
    this->setStyleSheet("chatterino--StreamPlayer, QWidget { background: #000; }"
                        "QPushButton { background: #18181d; color: #e6e6ea;"
                        " border: none; border-radius: 5px; padding: 3px 10px; }"
                        "QPushButton:hover { background: #26262d; }");
    this->audio_->setMuted(true);
    this->audio_->setVolume(0.4f);
    this->player_->setAudioOutput(this->audio_);
    this->player_->setVideoOutput(this->video_);

    auto *hide = new QPushButton("إخفاء البث", this->bar_);
    auto *barLayout = new QHBoxLayout(this->bar_);
    barLayout->setContentsMargins(6, 4, 6, 4);
    barLayout->addWidget(this->muteButton_);
    barLayout->addStretch(1);
    barLayout->addWidget(hide);
    this->bar_->setStyleSheet("background: #0f0f12;");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(this->video_, 1);
    layout->addWidget(this->bar_);

    auto updateMute = [this] {
        this->muteButton_->setText(this->audio_->isMuted() ? "🔇 تشغيل الصوت"
                                                           : "🔊 كتم الصوت");
    };
    updateMute();
    QObject::connect(this->muteButton_, &QPushButton::clicked, this,
                     [this, updateMute] {
                         this->audio_->setMuted(!this->audio_->isMuted());
                         updateMute();
                     });
    QObject::connect(hide, &QPushButton::clicked, this, [this] {
        this->dismissed_ = true;
        this->stop();
        this->setLive(false);
    });
    // stream ended / network hiccup: try again later
    QObject::connect(this->player_, &QMediaPlayer::errorOccurred, this, [this] {
        this->stop();
        this->retry_.start(30000);
    });

    this->retry_.setSingleShot(true);
    QObject::connect(&this->retry_, &QTimer::timeout, this, [this] {
        this->load();
    });
    this->setLive(false);
}

void StreamPlayer::setChannelName(const QString &login)
{
    if (login.compare(this->login_, Qt::CaseInsensitive) == 0)
    {
        return;
    }
    this->login_ = login;
    this->dismissed_ = false;
    this->stop();
    this->setLive(false);
    if (this->isVisible())
    {
        this->load();
    }
}

void StreamPlayer::load()
{
    this->retry_.stop();
    if (this->login_.isEmpty() || this->dismissed_ ||
        !viewerpresence::playerSetting().getValue())
    {
        this->stop();
        this->setLive(false);
        return;
    }
    int gen = ++this->generation_;
    viewerpresence::masterPlaylistUrl(
        this->login_, this, [this, gen](const QUrl &url) {
            if (gen != this->generation_)
            {
                return;  // channel changed meanwhile
            }
            if (url.isEmpty())
            {
                this->setLive(false);
                this->retry_.start(60000);
                return;
            }
            auto *reply = this->nam_->get(QNetworkRequest(url));
            QObject::connect(reply, &QNetworkReply::finished, this,
                             [this, reply, gen] {
                                 reply->deleteLater();
                                 auto variant =
                                     reply->error() == QNetworkReply::NoError
                                         ? pickVariant(reply->readAll())
                                         : QString();
                                 if (gen != this->generation_)
                                 {
                                     return;
                                 }
                                 if (variant.isEmpty())
                                 {
                                     this->setLive(false);
                                     this->retry_.start(60000);
                                     return;
                                 }
                                 this->setLive(true);
                                 this->player_->setSource(QUrl(variant));
                                 this->player_->play();
                             });
        });
}

void StreamPlayer::stop()
{
    ++this->generation_;
    this->player_->stop();
    this->player_->setSource({});
}

void StreamPlayer::setLive(bool live)
{
    this->live_ = live;
    this->updateHeight();
}

void StreamPlayer::updateHeight()
{
    int h = this->live_ ? std::min(this->width() * 9 / 16 + 34, 300) : 0;
    this->setFixedHeight(h);
}

void StreamPlayer::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    this->load();
}

void StreamPlayer::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    this->retry_.stop();
    this->stop();  // nothing plays in hidden tabs
}

void StreamPlayer::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    this->updateHeight();
}

}  // namespace chatterino
