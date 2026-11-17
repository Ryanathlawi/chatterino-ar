// SPDX-FileCopyrightText: 2023 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/twitch/LiveController.hpp"

#include "common/QLogging.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "util/Helpers.hpp"

#include <QApplication>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
const auto &LOG = chatterinoTwitchLiveController;

}  // namespace

namespace chatterino {

namespace {

// Arabic build: Helix needs a login, so signed-out users never learned who
// is live. Twitch's public GQL answers the same question without one.
void fetchStreamsGql(
    const QStringList &ids,
    const std::function<void(const std::vector<HelixStream> &)> &done)
{
    static auto *nam = new QNetworkAccessManager(qApp);
    QJsonObject body{
        {"query", "query($ids:[ID!]){users(ids:$ids){id stream{id title "
                  "viewersCount createdAt type game{id name}}}}"},
        {"variables",
         QJsonObject{{"ids", QJsonArray::fromStringList(ids)}}}};
    QNetworkRequest req(QUrl("https://gql.twitch.tv/gql"));
    req.setRawHeader("Client-ID", "kimne78kx3ncx6brgo4mv6wki5h1ko");
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    auto *reply =
        nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QObject::connect(reply, &QNetworkReply::finished, qApp, [reply, done] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
        {
            qCWarning(LOG) << "Failed GQL stream check request";
            return;
        }
        std::vector<HelixStream> streams;
        auto users = QJsonDocument::fromJson(reply->readAll())["data"]["users"]
                         .toArray();
        for (const auto &u : users)
        {
            auto user = u.toObject();
            auto s = user["stream"].toObject();
            if (s.isEmpty())
            {
                continue;
            }
            HelixStream h;
            h.id = s["id"].toString();
            h.userId = user["id"].toString();
            h.title = s["title"].toString();
            h.viewerCount = s["viewersCount"].toInt();
            h.startedAt = s["createdAt"].toString();
            h.type = s["type"].toString();
            h.gameId = s["game"].toObject()["id"].toString();
            h.gameName = s["game"].toObject()["name"].toString();
            streams.push_back(h);
        }
        done(streams);
    });
}

}  // namespace

TwitchLiveController::TwitchLiveController()
{
    QObject::connect(&this->refreshTimer, &QTimer::timeout, [this] {
        this->request();
    });
    this->refreshTimer.start(TwitchLiveController::REFRESH_INTERVAL);

    QObject::connect(&this->immediateRequestTimer, &QTimer::timeout, [this] {
        QStringList channelIDs;

        {
            std::unique_lock immediateRequestsLock(
                this->immediateRequestsMutex);
            for (const auto &channelID : this->immediateRequests)
            {
                channelIDs.append(channelID);
            }
            this->immediateRequests.clear();
        }

        if (channelIDs.isEmpty())
        {
            return;
        }

        this->request(channelIDs);
    });
    this->immediateRequestTimer.start(
        TwitchLiveController::IMMEDIATE_REQUEST_INTERVAL);
}

void TwitchLiveController::add(const std::shared_ptr<TwitchChannel> &newChannel)
{
    assert(newChannel != nullptr);

    const auto channelID = newChannel->roomId();
    assert(!channelID.isEmpty());

    {
        std::unique_lock lock(this->channelsMutex);
        this->channels[channelID] = {.ptr = newChannel, .wasChecked = false};
    }

    {
        std::unique_lock immediateRequestsLock(this->immediateRequestsMutex);
        this->immediateRequests.emplace(channelID);
    }
}

void TwitchLiveController::request(std::optional<QStringList> optChannelIDs)
{
    QStringList channelIDs;

    if (optChannelIDs)
    {
        channelIDs = *optChannelIDs;
    }
    else
    {
        std::shared_lock lock(this->channelsMutex);

        for (const auto &channelList : this->channels)
        {
            channelIDs.append(channelList.first);
        }
    }

    if (channelIDs.isEmpty())
    {
        return;
    }

    auto batches =
        splitListIntoBatches(channelIDs, TwitchLiveController::BATCH_SIZE);

    qCDebug(LOG) << "Make" << batches.size() << "requests";

    for (const auto &batch : batches)
    {
        auto applyStreams = [this, batch{batch}](
                                const std::vector<HelixStream> &streams) {
                std::unordered_map<QString, std::optional<HelixStream>> results;

                for (const auto &channelID : batch)
                {
                    results[channelID] = std::nullopt;
                }

                for (const auto &stream : streams)
                {
                    results[stream.userId] = stream;
                }

                QStringList deadChannels;

                {
                    std::shared_lock lock(this->channelsMutex);
                    for (const auto &result : results)
                    {
                        auto it = this->channels.find(result.first);
                        if (it != this->channels.end())
                        {
                            if (auto channel = it->second.ptr.lock(); channel)
                            {
                                channel->updateStreamStatus(
                                    result.second, !it->second.wasChecked);
                                it->second.wasChecked = true;
                            }
                            else
                            {
                                deadChannels.append(result.first);
                            }
                        }
                    }
                }

                if (!deadChannels.isEmpty())
                {
                    std::unique_lock lock(this->channelsMutex);
                    for (const auto &deadChannel : deadChannels)
                    {
                        this->channels.erase(deadChannel);
                    }
                }
        };

        // TODO: Explore making this concurrent
        getHelix()->fetchStreams(
            batch, {}, applyStreams,
            [applyStreams, batch{batch}] {
                fetchStreamsGql(batch, applyStreams);
            },
            [] {});

        // TODO: Explore making this concurrent
        getHelix()->fetchChannels(
            batch,
            [this, batch{batch}](const auto &helixChannels) {
                QStringList deadChannels;

                {
                    std::shared_lock lock(this->channelsMutex);
                    for (const auto &helixChannel : helixChannels)
                    {
                        auto it = this->channels.find(helixChannel.userId);
                        if (it != this->channels.end())
                        {
                            if (auto channel = it->second.ptr.lock(); channel)
                            {
                                channel->updateStreamTitle(helixChannel.title);
                                channel->updateDisplayName(helixChannel.name);
                            }
                            else
                            {
                                deadChannels.append(helixChannel.userId);
                            }
                        }
                    }
                }

                if (!deadChannels.isEmpty())
                {
                    std::unique_lock lock(this->channelsMutex);
                    for (const auto &deadChannel : deadChannels)
                    {
                        this->channels.erase(deadChannel);
                    }
                }
            },
            [] {
                qCWarning(LOG) << "Failed stream check request";
            });
    }
}

}  // namespace chatterino
