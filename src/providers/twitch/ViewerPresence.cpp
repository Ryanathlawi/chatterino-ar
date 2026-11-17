#include "providers/twitch/ViewerPresence.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
#include "singletons/WindowManager.hpp"
#include "widgets/Notebook.hpp"
#include "widgets/splits/Split.hpp"
#include "widgets/splits/SplitContainer.hpp"
#include "widgets/Window.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRandomGenerator>
#include <QSet>
#include <QTimer>
#include <QUrlQuery>

#include <map>
#include <memory>

namespace chatterino::viewerpresence {

namespace {

// public web client id used by twitch.tv's own player for anonymous playback
const QByteArray WEB_CLIENT_ID = "kimne78kx3ncx6brgo4mv6wki5h1ko";

QNetworkAccessManager *nam()
{
    static auto *n = new QNetworkAccessManager;
    return n;
}

}  // namespace

void masterPlaylistUrl(const QString &login, QObject *ctx,
                       std::function<void(const QUrl &)> done)
{
    QJsonObject vars{{"isLive", true},     {"login", login},
                     {"isVod", false},     {"vodID", ""},
                     {"playerType", "embed"}};  // "site" gets stitched ads, "embed" does not
    QJsonObject body{
        {"operationName", "PlaybackAccessToken_Template"},
        {"query",
         "query PlaybackAccessToken_Template($login: String!, $isLive: "
         "Boolean!, $vodID: ID!, $isVod: Boolean!, $playerType: String!) "
         "{ streamPlaybackAccessToken(channelName: $login, params: "
         "{platform: \"web\", playerBackend: \"mediaplayer\", playerType: "
         "$playerType}) @include(if: $isLive) { value signature } "
         "videoPlaybackAccessToken(id: $vodID, params: {platform: "
         "\"web\", playerBackend: \"mediaplayer\", playerType: "
         "$playerType}) @include(if: $isVod) { value signature } }"},
        {"variables", vars}};
    QNetworkRequest req(QUrl("https://gql.twitch.tv/gql"));
    req.setRawHeader("Client-ID", WEB_CLIENT_ID);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    auto *reply =
        nam()->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QObject::connect(reply, &QNetworkReply::finished, ctx,
                     [login, reply, done = std::move(done)] {
                         reply->deleteLater();
                         auto tok = QJsonDocument::fromJson(reply->readAll())
                                        .object()["data"]
                                        .toObject()["streamPlaybackAccessToken"]
                                        .toObject();
                         if (tok["value"].toString().isEmpty())
                         {
                             return done({});
                         }
                         QUrl url("https://usher.ttvnw.net/api/channel/hls/" +
                                  login.toLower() + ".m3u8");
                         QUrlQuery q;
                         q.addQueryItem("sig", tok["signature"].toString());
                         q.addQueryItem("token", tok["value"].toString());
                         q.addQueryItem("allow_source", "true");
                         q.addQueryItem("allow_audio_only", "true");
                         q.addQueryItem("player", "twitchweb");
                         q.addQueryItem(
                             "p", QString::number(
                                      QRandomGenerator::global()->bounded(
                                          9999999)));
                         url.setQuery(q);
                         done(url);
                     });
}

namespace {

// One channel: token -> master playlist -> lowest variant -> poll segments.
class Watcher : public QObject
{
public:
    explicit Watcher(QString login)
        : login_(std::move(login))
    {
        this->timer_.setSingleShot(true);
        QObject::connect(&this->timer_, &QTimer::timeout, this, [this] {
            this->step();
        });
        this->step();
    }

private:
    void retryIn(int ms, bool resetToken)
    {
        if (resetToken)
        {
            this->variantUrl_.clear();
        }
        this->timer_.start(ms);
    }

    void step()
    {
        this->variantUrl_.isEmpty() ? this->fetchToken() : this->poll();
    }

    void fetchToken()
    {
        masterPlaylistUrl(this->login_, this, [this](const QUrl &url) {
            url.isEmpty() ? this->retryIn(60000, true) : this->fetchMaster(url);
        });
    }

    void fetchMaster(const QUrl &url)
    {
        auto *reply = nam()->get(QNetworkRequest(url));
        QObject::connect(reply, &QNetworkReply::finished, this, [this, reply] {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError)
            {
                // 404 = offline; check again in a minute
                return this->retryIn(60000, true);
            }
            // pick the lowest-bandwidth variant
            qint64 best = -1;
            QString bestUrl;
            qint64 bw = 0;
            for (const auto &line : QString(reply->readAll()).split('\n'))
            {
                auto l = line.trimmed();
                if (l.startsWith("#EXT-X-STREAM-INF"))
                {
                    auto i = l.indexOf("BANDWIDTH=");
                    bw = i < 0 ? 0 : l.mid(i + 10).section(',', 0, 0).toLongLong();
                }
                else if (!l.isEmpty() && !l.startsWith('#'))
                {
                    if (best < 0 || bw < best)
                    {
                        best = bw;
                        bestUrl = l;
                    }
                }
            }
            if (bestUrl.isEmpty())
            {
                return this->retryIn(60000, true);
            }
            this->variantUrl_ = bestUrl;
            this->poll();
        });
    }

    void poll()
    {
        auto *reply = nam()->get(QNetworkRequest(QUrl(this->variantUrl_)));
        QObject::connect(reply, &QNetworkReply::finished, this, [this, reply] {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError)
            {
                // token expired or stream ended: start over
                return this->retryIn(5000, true);
            }
            QString last;
            for (const auto &line : QString(reply->readAll()).split('\n'))
            {
                auto l = line.trimmed();
                if (!l.isEmpty() && !l.startsWith('#'))
                {
                    last = l;
                }
            }
            if (!last.isEmpty() && last != this->lastSegment_)
            {
                this->lastSegment_ = last;
                // download and drop the bytes as they arrive
                auto *seg = nam()->get(QNetworkRequest(QUrl(last)));
                QObject::connect(seg, &QNetworkReply::readyRead, seg, [seg] {
                    seg->skip(seg->bytesAvailable());
                });
                QObject::connect(seg, &QNetworkReply::finished, seg,
                                 &QObject::deleteLater);
            }
            this->retryIn(2000, false);
        });
    }

    QString login_;
    QString variantUrl_;
    QString lastSegment_;
    QTimer timer_;
};

std::map<QString, std::unique_ptr<Watcher>> &watchers()
{
    static std::map<QString, std::unique_ptr<Watcher>> w;
    return w;
}

QSet<QString> visibleChannels()
{
    QSet<QString> out;
    auto &window = getApp()->getWindows()->getMainWindow();
    if (auto *page = window.getNotebook().getSelectedPage())
    {
        for (auto *split : page->getSplits())
        {
            auto ch = split->getChannel();
            if (ch && ch->isTwitchChannel() && !ch->getName().isEmpty())
            {
                out.insert(ch->getName().toLower());
            }
        }
    }
    return out;
}

void sync()
{
    auto want = enabledSetting().getValue() ? visibleChannels()
                                             : QSet<QString>{};
    auto &w = watchers();
    for (auto it = w.begin(); it != w.end();)
    {
        it = want.contains(it->first) ? std::next(it) : w.erase(it);
    }
    for (const auto &login : want)
    {
        if (!w.contains(login))
        {
            w.emplace(login, std::make_unique<Watcher>(login));
        }
    }
}

}  // namespace

BoolSetting &playerSetting()
{
    static BoolSetting s{"/arabic/showPlayer", true};
    return s;
}

BoolSetting &enabledSetting()
{
    static BoolSetting s{"/arabic/countAsViewer", true};
    return s;
}

void start()
{
    // ponytail: 3s poll of the visible splits instead of wiring every
    // tab/split signal; cheap (a few pointer reads)
    auto *t = new QTimer;
    QObject::connect(t, &QTimer::timeout, [] {
        sync();
    });
    t->start(3000);
    sync();
}

}  // namespace chatterino::viewerpresence
