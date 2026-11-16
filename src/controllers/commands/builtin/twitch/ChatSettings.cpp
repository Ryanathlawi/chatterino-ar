// SPDX-FileCopyrightText: 2022 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/commands/builtin/twitch/ChatSettings.hpp"

#include "Application.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/commands/CommandContext.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "util/FormatTime.hpp"
#include "util/Helpers.hpp"

namespace {

using namespace chatterino;

QString formatError(const HelixUpdateChatSettingsError error,
                    const QString &message, int durationUnitMultiplier = 1)
{
    static const QRegularExpression invalidRange("(\\d+) through (\\d+)");

    QString errorMessage = QString("فشل التحديث - ");
    using Error = HelixUpdateChatSettingsError;
    switch (error)
    {
        case Error::UserMissingScope: {
            // TODO(pajlada): Phrase MISSING_REQUIRED_SCOPE
            errorMessage += "صلاحية ناقصة. سجّل دخولك من جديد وحاول مرة ثانية.";
        }
        break;

        case Error::UserNotAuthorized:
        case Error::Forbidden: {
            // TODO(pajlada): Phrase MISSING_PERMISSION
            errorMessage += "ما عندك صلاحية تسوي هذا الإجراء.";
        }
        break;

        case Error::Ratelimited: {
            errorMessage += "Twitch حاط عليك حد مؤقت. حاول بعد ثواني.";
        }
        break;

        case Error::OutOfRange: {
            QRegularExpressionMatch matched = invalidRange.match(message);
            if (matched.hasMatch())
            {
                auto from = matched.captured(1).toInt();
                auto to = matched.captured(2).toInt();
                errorMessage +=
                    QString("المدة خارج النطاق المسموح: من %1 إلى %2.")
                        .arg(from == 0
                                 ? "0s"
                                 : formatTime(from * durationUnitMultiplier),
                             to == 0 ? "0s"
                                     : formatTime(to * durationUnitMultiplier));
            }
            else
            {
                errorMessage += message;
            }
        }
        break;

        case Error::Forwarded: {
            errorMessage = message;
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

// Do nothing as we'll receive a message from IRC about updates
auto successCallback = [](auto result) {};

auto failureCallback = [](ChannelPtr channel, int durationUnitMultiplier = 1) {
    return [channel, durationUnitMultiplier](const auto &error,
                                             const QString &message) {
        channel->addSystemMessage(
            formatError(error, message, durationUnitMultiplier));
    };
};

const auto P_NOT_LOGGED_IN =
    QStringLiteral("لازم تسجّل دخول عشان تغيّر إعدادات الشات!");

}  // namespace

namespace chatterino::commands {

QString emoteOnly(const CommandContext &ctx)
{
    auto currentUser = getApp()->getAccounts()->twitch.getCurrent();
    if (currentUser->isAnon())
    {
        ctx.channel->addSystemMessage(P_NOT_LOGGED_IN);
        return "";
    }

    if (ctx.twitchChannel == nullptr)
    {
        ctx.channel->addSystemMessage(
            "الأمر /emoteonly يشتغل بس في قنوات Twitch.");
        return "";
    }

    if (ctx.twitchChannel->accessRoomModes()->emoteOnly)
    {
        ctx.channel->addSystemMessage(
            "الشات أصلًا في وضع الإيموتات فقط.");
        return "";
    }

    getHelix()->updateEmoteMode(ctx.twitchChannel->roomId(),
                                currentUser->getUserId(), true, successCallback,
                                failureCallback(ctx.channel));

    return "";
}

QString emoteOnlyOff(const CommandContext &ctx)
{
    auto currentUser = getApp()->getAccounts()->twitch.getCurrent();
    if (currentUser->isAnon())
    {
        ctx.channel->addSystemMessage(P_NOT_LOGGED_IN);
        return "";
    }
    if (ctx.twitchChannel == nullptr)
    {
        ctx.channel->addSystemMessage(
            "الأمر /emoteonlyoff يشتغل بس في قنوات Twitch.");
        return "";
    }

    if (!ctx.twitchChannel->accessRoomModes()->emoteOnly)
    {
        ctx.channel->addSystemMessage("الشات مو في وضع الإيموتات فقط.");
        return "";
    }

    getHelix()->updateEmoteMode(ctx.twitchChannel->roomId(),
                                currentUser->getUserId(), false,
                                successCallback, failureCallback(ctx.channel));

    return "";
}

QString subscribers(const CommandContext &ctx)
{
    auto currentUser = getApp()->getAccounts()->twitch.getCurrent();
    if (currentUser->isAnon())
    {
        ctx.channel->addSystemMessage(P_NOT_LOGGED_IN);
        return "";
    }

    if (ctx.twitchChannel == nullptr)
    {
        ctx.channel->addSystemMessage(
            "الأمر /subscribers يشتغل بس في قنوات Twitch.");
        return "";
    }

    if (ctx.twitchChannel->accessRoomModes()->submode)
    {
        ctx.channel->addSystemMessage(
            "الشات أصلًا للمشتركين فقط.");
        return "";
    }

    getHelix()->updateSubscriberMode(
        ctx.twitchChannel->roomId(), currentUser->getUserId(), true,
        successCallback, failureCallback(ctx.channel));

    return "";
}

QString subscribersOff(const CommandContext &ctx)
{
    auto currentUser = getApp()->getAccounts()->twitch.getCurrent();
    if (currentUser->isAnon())
    {
        ctx.channel->addSystemMessage(P_NOT_LOGGED_IN);
        return "";
    }

    if (ctx.twitchChannel == nullptr)
    {
        ctx.channel->addSystemMessage(
            "الأمر /subscribersoff يشتغل بس في قنوات Twitch.");
        return "";
    }

    if (!ctx.twitchChannel->accessRoomModes()->submode)
    {
        ctx.channel->addSystemMessage(
            "الشات مو للمشتركين فقط.");
        return "";
    }

    getHelix()->updateSubscriberMode(
        ctx.twitchChannel->roomId(), currentUser->getUserId(), false,
        successCallback, failureCallback(ctx.channel));

    return "";
}

QString slow(const CommandContext &ctx)
{
    auto currentUser = getApp()->getAccounts()->twitch.getCurrent();
    if (currentUser->isAnon())
    {
        ctx.channel->addSystemMessage(P_NOT_LOGGED_IN);
        return "";
    }

    if (ctx.twitchChannel == nullptr)
    {
        ctx.channel->addSystemMessage(
            "الأمر /slow يشتغل بس في قنوات Twitch.");
        return "";
    }

    int duration = 30;
    if (ctx.words.length() >= 2)
    {
        bool ok = false;
        duration = ctx.words.at(1).toInt(&ok);
        if (!ok || duration <= 0)
        {
            ctx.channel->addSystemMessage(
                "الاستخدام: \"/slow [المدة]\" - يفعّل الوضع البطيء (يحدد كم مرة يقدر المستخدم يرسل). المدة اختيارية (الافتراضي 30) بالثواني. استخدم \"slowoff\" للإيقاف.");
            return "";
        }
    }

    if (ctx.twitchChannel->accessRoomModes()->slowMode == duration)
    {
        ctx.channel->addSystemMessage(
            QString("الشات أصلًا في الوضع البطيء %1 ثانية.")
                .arg(duration));
        return "";
    }

    getHelix()->updateSlowMode(ctx.twitchChannel->roomId(),
                               currentUser->getUserId(), duration,
                               successCallback, failureCallback(ctx.channel));

    return "";
}

QString slowOff(const CommandContext &ctx)
{
    auto currentUser = getApp()->getAccounts()->twitch.getCurrent();
    if (currentUser->isAnon())
    {
        ctx.channel->addSystemMessage(P_NOT_LOGGED_IN);
        return "";
    }

    if (ctx.twitchChannel == nullptr)
    {
        ctx.channel->addSystemMessage(
            "الأمر /slowoff يشتغل بس في قنوات Twitch.");
        return "";
    }

    if (ctx.twitchChannel->accessRoomModes()->slowMode <= 0)
    {
        ctx.channel->addSystemMessage("الشات مو في الوضع البطيء.");
        return "";
    }

    getHelix()->updateSlowMode(ctx.twitchChannel->roomId(),
                               currentUser->getUserId(), std::nullopt,
                               successCallback, failureCallback(ctx.channel));

    return "";
}

QString followers(const CommandContext &ctx)
{
    auto currentUser = getApp()->getAccounts()->twitch.getCurrent();
    if (currentUser->isAnon())
    {
        ctx.channel->addSystemMessage(P_NOT_LOGGED_IN);
        return "";
    }

    if (ctx.twitchChannel == nullptr)
    {
        ctx.channel->addSystemMessage(
            "الأمر /followers يشتغل بس في قنوات Twitch.");
        return "";
    }

    int duration = 0;
    if (ctx.words.length() >= 2)
    {
        auto parsed = parseDurationToSeconds(ctx.words.mid(1).join(' '), 60);
        duration = (int)(parsed / 60);
        // -1 / 60 == 0 => use parsed
        if (parsed < 0)
        {
            ctx.channel->addSystemMessage(
                "الاستخدام: \"/followers [المدة]\" - يخلي الشات للمتابعين بس (اللي متابعين من مدة معينة). أمثلة: \"30m\"، \"1 week\"، \"5 days 12 hours\". أقل من 3 شهور.");
            return "";
        }
    }

    if (ctx.twitchChannel->accessRoomModes()->followerOnly == duration)
    {
        ctx.channel->addSystemMessage(
            QString("الشات أصلًا للمتابعين فقط (%1).")
                .arg(formatTime(duration * 60)));
        return "";
    }

    getHelix()->updateFollowerMode(
        ctx.twitchChannel->roomId(), currentUser->getUserId(), duration,
        successCallback, failureCallback(ctx.channel, 60));

    return "";
}

QString followersOff(const CommandContext &ctx)
{
    auto currentUser = getApp()->getAccounts()->twitch.getCurrent();
    if (currentUser->isAnon())
    {
        ctx.channel->addSystemMessage(P_NOT_LOGGED_IN);
        return "";
    }

    if (ctx.twitchChannel == nullptr)
    {
        ctx.channel->addSystemMessage(
            "الأمر /followersoff يشتغل بس في قنوات Twitch.");
        return "";
    }

    if (ctx.twitchChannel->accessRoomModes()->followerOnly < 0)
    {
        ctx.channel->addSystemMessage(
            "الشات مو للمتابعين فقط. ");
        return "";
    }

    getHelix()->updateFollowerMode(
        ctx.twitchChannel->roomId(), currentUser->getUserId(), std::nullopt,
        successCallback, failureCallback(ctx.channel));

    return "";
}

QString uniqueChat(const CommandContext &ctx)
{
    auto currentUser = getApp()->getAccounts()->twitch.getCurrent();
    if (currentUser->isAnon())
    {
        ctx.channel->addSystemMessage(P_NOT_LOGGED_IN);
        return "";
    }

    if (ctx.twitchChannel == nullptr)
    {
        ctx.channel->addSystemMessage(
            "الأمر /uniquechat يشتغل بس في قنوات Twitch.");
        return "";
    }

    if (ctx.twitchChannel->accessRoomModes()->r9k)
    {
        ctx.channel->addSystemMessage(
            "الشات أصلًا في وضع الرسائل الفريدة.");
        return "";
    }

    getHelix()->updateUniqueChatMode(
        ctx.twitchChannel->roomId(), currentUser->getUserId(), true,
        successCallback, failureCallback(ctx.channel));

    return "";
}

QString uniqueChatOff(const CommandContext &ctx)
{
    auto currentUser = getApp()->getAccounts()->twitch.getCurrent();
    if (currentUser->isAnon())
    {
        ctx.channel->addSystemMessage(P_NOT_LOGGED_IN);
        return "";
    }

    if (ctx.twitchChannel == nullptr)
    {
        ctx.channel->addSystemMessage(
            "الأمر /uniquechatoff يشتغل بس في قنوات Twitch.");
        return "";
    }

    if (!ctx.twitchChannel->accessRoomModes()->r9k)
    {
        ctx.channel->addSystemMessage("الشات مو في وضع الرسائل الفريدة.");
        return "";
    }

    getHelix()->updateUniqueChatMode(
        ctx.twitchChannel->roomId(), currentUser->getUserId(), false,
        successCallback, failureCallback(ctx.channel));

    return "";
}

}  // namespace chatterino::commands
