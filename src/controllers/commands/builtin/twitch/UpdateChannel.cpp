// SPDX-FileCopyrightText: 2023 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/commands/builtin/twitch/UpdateChannel.hpp"

#include "common/network/NetworkResult.hpp"
#include "controllers/commands/CommandContext.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/TwitchChannel.hpp"

namespace {

using namespace chatterino;

QString formatUpdateChannelError(const char *updateType,
                                 HelixUpdateChannelError error,
                                 const QString &message)
{
    using Error = HelixUpdateChannelError;

    QString errorMessage = QString("فشل تعيين %1 - ").arg(updateType);

    switch (error)
    {
        case Error::UserMissingScope: {
            errorMessage += "صلاحية ناقصة. سجّل دخولك من جديد وحاول مرة ثانية.";
        }
        break;

        case Error::UserNotAuthorized: {
            errorMessage += QString("لازم تكون صاحب القناة عشان تغيّر %1.")
                                .arg(updateType);
        }
        break;

        case Error::Ratelimited: {
            errorMessage += "Twitch حاط عليك حد مؤقت. حاول بعد ثواني.";
        }
        break;

        case Error::Forwarded: {
            errorMessage += message;
        }
        break;

        case Error::Unknown:
        default: {
            errorMessage +=
                QString("صار خطأ غير معروف (%1).").arg(message);
        }
        break;
    }

    return errorMessage;
}

}  // namespace

namespace chatterino::commands {

QString setTitle(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    if (ctx.words.size() < 2)
    {
        ctx.channel->addSystemMessage("الاستخدام: /settitle <عنوان البث>");
        return "";
    }

    if (ctx.twitchChannel == nullptr)
    {
        ctx.channel->addSystemMessage(
            "ما تقدر تغيّر عنوان قناة مو على Twitch.");
        return "";
    }

    auto title = ctx.words.mid(1).join(" ");

    getHelix()->updateChannel(
        ctx.twitchChannel->roomId(), "", "", title,
        [channel{ctx.channel}, title](const auto &result) {
            (void)result;

            channel->addSystemMessage(
                QString("تم تغيير العنوان إلى %1").arg(title));
        },
        [channel{ctx.channel}](auto error, auto message) {
            auto errorMessage =
                formatUpdateChannelError("title", error, message);
            channel->addSystemMessage(errorMessage);
        });

    return "";
}

QString setGame(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    if (ctx.words.size() < 2)
    {
        ctx.channel->addSystemMessage("الاستخدام: /setgame <اللعبة>");
        return "";
    }

    if (ctx.twitchChannel == nullptr)
    {
        ctx.channel->addSystemMessage(
            "ما تقدر تغيّر لعبة قناة مو على Twitch.");
        return "";
    }

    const auto gameName = ctx.words.mid(1).join(" ");

    getHelix()->searchGames(
        gameName,
        [channel{ctx.channel}, twitchChannel{ctx.twitchChannel},
         gameName](const std::vector<HelixGame> &games) {
            if (games.empty())
            {
                channel->addSystemMessage("ما لقينا اللعبة.");
                return;
            }

            auto matchedGame = games.at(0);

            if (games.size() > 1)
            {
                // NOTE: Improvements could be made with 'fuzzy string matching' code here
                // attempt to find the best looking game by comparing exactly with lowercase values
                for (const auto &game : games)
                {
                    if (game.name.toLower() == gameName.toLower())
                    {
                        matchedGame = game;
                        break;
                    }
                }
            }

            auto status = twitchChannel->accessStreamStatus();
            getHelix()->updateChannel(
                twitchChannel->roomId(), matchedGame.id, "", "",
                [channel, games, matchedGame](const NetworkResult &) {
                    channel->addSystemMessage(
                        QString("تم تغيير اللعبة إلى %1").arg(matchedGame.name));
                },
                [channel](auto error, auto message) {
                    auto errorMessage =
                        formatUpdateChannelError("game", error, message);
                    channel->addSystemMessage(errorMessage);
                });
        },
        [channel{ctx.channel}] {
            channel->addSystemMessage("فشل البحث عن اللعبة.");
        });

    return "";
}

}  // namespace chatterino::commands
