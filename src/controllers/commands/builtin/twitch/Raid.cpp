// SPDX-FileCopyrightText: 2023 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/commands/builtin/twitch/Raid.hpp"

#include "Application.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/commands/CommandContext.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "util/Twitch.hpp"

namespace {

using namespace chatterino;

QString formatStartRaidError(HelixStartRaidError error, const QString &message)
{
    QString errorMessage = QString("فشل بدء الغارة - ");

    using Error = HelixStartRaidError;

    switch (error)
    {
        case Error::UserMissingScope: {
            // TODO(pajlada): Phrase MISSING_REQUIRED_SCOPE
            errorMessage += "صلاحية ناقصة. سجّل دخولك من جديد وحاول مرة ثانية.";
        }
        break;

        case Error::UserNotAuthorized: {
            errorMessage += "لازم تكون صاحب القناة عشان تبدأ غارة.";
        }
        break;

        case Error::CantRaidYourself: {
            errorMessage += "القناة ما تقدر تسوي غارة على نفسها.";
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
            errorMessage += "صار خطأ غير معروف.";
        }
        break;
    }

    return errorMessage;
}

QString formatCancelRaidError(HelixCancelRaidError error,
                              const QString &message)
{
    QString errorMessage = QString("فشل إلغاء الغارة - ");

    using Error = HelixCancelRaidError;

    switch (error)
    {
        case Error::UserMissingScope: {
            // TODO(pajlada): Phrase MISSING_REQUIRED_SCOPE
            errorMessage += "صلاحية ناقصة. سجّل دخولك من جديد وحاول مرة ثانية.";
        }
        break;

        case Error::UserNotAuthorized: {
            errorMessage += "لازم تكون صاحب القناة عشان تلغي الغارة.";
        }
        break;

        case Error::NoRaidPending: {
            errorMessage += "ما عندك غارة شغّالة.";
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
            errorMessage += "صار خطأ غير معروف.";
        }
        break;
    }

    return errorMessage;
}

}  // namespace

namespace chatterino::commands {

QString startRaid(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    if (ctx.twitchChannel == nullptr)
    {
        ctx.channel->addSystemMessage(
            "الأمر /raid يشتغل بس في قنوات Twitch.");
        return "";
    }

    if (ctx.words.size() < 2)
    {
        ctx.channel->addSystemMessage(
            "الاستخدام: \"/raid <المستخدم>\" - غارة على قناة. صاحب القناة بس يقدر يبدأها.");
        return "";
    }

    auto currentUser = getApp()->getAccounts()->twitch.getCurrent();
    if (currentUser->isAnon())
    {
        ctx.channel->addSystemMessage("لازم تسجّل دخول عشان تبدأ غارة!");
        return "";
    }

    auto target = ctx.words.at(1);
    stripChannelName(target);

    getHelix()->getUserByName(
        target,
        [twitchChannel{ctx.twitchChannel},
         channel{ctx.channel}](const HelixUser &targetUser) {
            getHelix()->startRaid(
                twitchChannel->roomId(), targetUser.id,
                [] {
                    // do nothing
                },
                [channel, targetUser](auto error, auto message) {
                    auto errorMessage = formatStartRaidError(error, message);
                    channel->addSystemMessage(errorMessage);
                });
        },
        [channel{ctx.channel}, target] {
            // Equivalent error from IRC
            channel->addSystemMessage(
                QString("اسم مستخدم غير صالح: %1").arg(target));
        });

    return "";
}

QString cancelRaid(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    if (ctx.twitchChannel == nullptr)
    {
        ctx.channel->addSystemMessage(
            "الأمر /unraid يشتغل بس في قنوات Twitch.");
        return "";
    }

    if (ctx.words.size() != 1)
    {
        ctx.channel->addSystemMessage(
            "الاستخدام: \"/unraid\" - يلغي الغارة الحالية. صاحب القناة بس يقدر يلغيها.");
        return "";
    }

    auto currentUser = getApp()->getAccounts()->twitch.getCurrent();
    if (currentUser->isAnon())
    {
        ctx.channel->addSystemMessage(
            "لازم تسجّل دخول عشان تلغي الغارة!");
        return "";
    }

    getHelix()->cancelRaid(
        ctx.twitchChannel->roomId(),
        [] {
            // do nothing
        },
        [channel{ctx.channel}](auto error, auto message) {
            auto errorMessage = formatCancelRaidError(error, message);
            channel->addSystemMessage(errorMessage);
        });

    return "";
}

}  // namespace chatterino::commands
