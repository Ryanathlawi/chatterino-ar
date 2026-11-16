// SPDX-FileCopyrightText: 2025 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/commands/builtin/twitch/Poll.hpp"

#include "Application.hpp"
#include "common/QLogging.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/commands/CommandContext.hpp"
#include "controllers/commands/common/ChannelAction.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchChannel.hpp"

#include <chrono>

namespace {

using namespace chatterino;

constexpr auto MIN_POLL_DURATION = std::chrono::seconds(10);
constexpr auto MAX_POLL_DURATION = std::chrono::seconds(1800);

}  // namespace

namespace chatterino::commands {

QString createPoll(const CommandContext &ctx)
{
    const auto command = QStringLiteral("/poll");
    const auto usage = QStringLiteral(
        R"(Usage: "/poll --title "<title>" --duration <duration>[time unit] --choice "<choice1>" --choice "<choice2>" [options...]" - Creates a poll for users to vote among the defined options. Title may not exceed 60 characters. There must be between two and five poll choices. Duration must be a positive integer; time unit (optional, default=s) must be one of s, m; maximum duration is 30 minutes. Options: --points <points> to allow spending the specified channel points for each additional vote.)");
    const auto action = parseUserParticipationAction(
        ctx, command, usage, MIN_POLL_DURATION, MAX_POLL_DURATION);

    if (!action.has_value())
    {
        if (ctx.channel != nullptr)
        {
            ctx.channel->addSystemMessage(action.error());
        }
        else
        {
            qCWarning(chatterinoCommands)
                << "Error parsing command:" << action.error();
        }
        return "";
    }

    auto currentUser = getApp()->getAccounts()->twitch.getCurrent();
    if (currentUser->isAnon())
    {
        ctx.channel->addSystemMessage(
            "لازم تسجّل دخول عشان تسوي تصويت!");
        return "";
    }

    const auto &poll = action.value();
    getHelix()->createPoll(
        poll.broadcasterID, poll.title, poll.choices, poll.duration,
        poll.pointsPerVote,
        [channel = ctx.channel, poll] {
            channel->addSystemMessage(
                QString("تم إنشاء التصويت: '%1'").arg(poll.title));
        },
        [channel = ctx.channel](const auto &error) {
            channel->addSystemMessage("فشل إنشاء التصويت - " + error);
        });

    return "";
}

QString endPoll(const CommandContext &ctx)
{
    if (ctx.twitchChannel == nullptr)
    {
        const auto err = QStringLiteral(
            "الأمر /endpoll يشتغل بس في قنوات Twitch.");
        if (ctx.channel != nullptr)
        {
            ctx.channel->addSystemMessage(err);
        }
        else
        {
            qCWarning(chatterinoCommands) << "Invalid command context:" << err;
        }
        return "";
    }

    auto currentUser = getApp()->getAccounts()->twitch.getCurrent();
    if (currentUser->isAnon())
    {
        ctx.channel->addSystemMessage("لازم تسجّل دخول عشان تنهي التصويت!");
        return "";
    }

    const auto roomId = ctx.twitchChannel->roomId();
    getHelix()->getPolls(
        roomId, {}, 1, {},
        [channel = ctx.channel, roomId](const auto &result) {
            if (result.polls.empty())
            {
                channel->addSystemMessage("ما لقينا أي تصويت");
                return;
            }

            auto poll = result.polls.front();
            if (poll.status != "ACTIVE")
            {
                channel->addSystemMessage("ما فيه تصويت شغّال");
                return;
            }

            getHelix()->endPoll(
                roomId, poll.id, false,
                [channel](const HelixPoll &data) {
                    // find most popular choice
                    HelixPollChoice winner = data.choices.front();
                    int totalVotes = 0;
                    int winnerCount = 0;
                    for (const auto &choice : data.choices)
                    {
                        totalVotes += choice.votes;
                        if (choice.votes > winner.votes)
                        {
                            winner = choice;
                            winnerCount = 1;
                        }
                        else if (choice.votes == winner.votes)
                        {
                            winnerCount++;
                        }
                    }

                    if (totalVotes == 0)
                    {
                        channel->addSystemMessage(
                            QString("انتهى التصويت بدون أصوات: '%1'")
                                .arg(data.title));
                        return;
                    }

                    if (winnerCount > 1)
                    {
                        channel->addSystemMessage(
                            QString("انتهى التصويت بالتعادل: '%1'")
                                .arg(data.title));
                        return;
                    }

                    const double percent =
                        100.0 * winner.votes / std::max(totalVotes, 1);

                    channel->addSystemMessage(
                        QString(
                            "انتهى التصويت: '%1' - فاز '%2' بـ %3 صوت (%4%)")
                            .arg(data.title, winner.title,
                                 QString::number(winner.votes),
                                 QString::number(percent, 'f', 1)));
                },
                [channel](const auto &error) {
                    channel->addSystemMessage("فشل إنهاء التصويت - " +
                                              error);
                });
        },
        [channel = ctx.channel](const auto &error) {
            channel->addSystemMessage("فشل جلب التصويتات - " + error);
        });

    return "";
}

QString cancelPoll(const CommandContext &ctx)
{
    if (ctx.twitchChannel == nullptr)
    {
        const auto err = QStringLiteral(
            "الأمر /cancelpoll يشتغل بس في قنوات Twitch.");
        if (ctx.channel != nullptr)
        {
            ctx.channel->addSystemMessage(err);
        }
        else
        {
            qCWarning(chatterinoCommands) << "Invalid command context:" << err;
        }
        return "";
    }

    auto currentUser = getApp()->getAccounts()->twitch.getCurrent();
    if (currentUser->isAnon())
    {
        ctx.channel->addSystemMessage(
            "لازم تسجّل دخول عشان تلغي التصويت!");
        return "";
    }

    const auto roomId = ctx.twitchChannel->roomId();
    getHelix()->getPolls(
        roomId, {}, 1, {},
        [channel = ctx.channel, roomId](const auto &result) {
            if (result.polls.empty())
            {
                channel->addSystemMessage("ما لقينا أي تصويت");
                return;
            }

            auto poll = result.polls.front();
            if (poll.status != "ACTIVE")
            {
                channel->addSystemMessage("ما فيه تصويت شغّال");
                return;
            }

            getHelix()->endPoll(
                roomId, poll.id, true,
                [channel](const HelixPoll &data) {
                    channel->addSystemMessage(
                        QString("تم إلغاء التصويت: '%1'").arg(data.title));
                },
                [channel](const auto &error) {
                    channel->addSystemMessage("فشل إلغاء التصويت - " +
                                              error);
                });
        },
        [channel = ctx.channel](const auto &error) {
            channel->addSystemMessage("فشل جلب التصويتات - " + error);
        });

    return "";
}

}  // namespace chatterino::commands
