// Arabic build: small live player above a split's chat (<=480p, starts muted).
// Plays only while visible; collapses when the channel is offline.
#pragma once

#include <QTimer>
#include <QWidget>

class QMediaPlayer;
class QAudioOutput;
class QVideoWidget;
class QPushButton;
class QNetworkAccessManager;

namespace chatterino {

class StreamPlayer : public QWidget
{
public:
    explicit StreamPlayer(QWidget *parent);

    void setChannelName(const QString &login);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void load();
    void stop();
    void setLive(bool live);
    void updateHeight();

    QString login_;
    bool live_ = false;
    bool dismissed_ = false;
    int generation_ = 0;
    QMediaPlayer *player_;
    QAudioOutput *audio_;
    QVideoWidget *video_;
    QPushButton *muteButton_;
    QWidget *bar_;
    QTimer retry_;
    QNetworkAccessManager *nam_;
};

}  // namespace chatterino
