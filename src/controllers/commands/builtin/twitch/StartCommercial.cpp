// SPDX-FileCopyrightText: 2023 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/commands/builtin/twitch/StartCommercial.hpp"

#include "Application.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/commands/CommandContext.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchChannel.hpp"

namespace {

using namespace chatterino;

QString formatStartCommercialError(HelixStartCommercialError error,
                                   const QString &message)
{
    using Error = HelixStartCommercialError;

    QString errorMessage = "فشل تشغيل الإعلان - ";

    switch (error)
    {
        case Error::UserMissingScope: {
            errorMessage += "صلاحية ناقصة. سجّل دخولك من جديد وحاول مرة ثانية.";
        }
        break;

        case Error::TokenMustMatchBroadcaster: {
            errorMessage += "صاحب القناة بس يقدر يشغّل إعلانات.";
        }
        break;

        case Error::BroadcasterNotStreaming: {
            errorMessage += "لازم تكون في بث مباشر عشان تشغّل إعلان.";
        }
        break;

        case Error::MissingLengthParameter: {
            errorMessage += "لازم تحدد مدة الإعلان وتكون أكبر من صفر.";
        }
        break;

        case Error::Ratelimited: {
            errorMessage += "لازم تنتظر لين تخلص فترة الانتظار قبل إعلان ثاني.";
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

QString startCommercial(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    if (ctx.twitchChannel == nullptr)
    {
        ctx.channel->addSystemMessage(
            "الأمر /commercial يشتغل بس في قنوات Twitch.");
        return "";
    }

    const auto *usageStr = "الاستخدام: \"/commercial <المدة>\" - يشغّل إعلان بالمدة المحددة. المدد المسموحة: 30، 60، 90، 120، 150، 180 ثانية.";

    if (ctx.words.size() < 2)
    {
        ctx.channel->addSystemMessage(usageStr);
        return "";
    }

    auto user = getApp()->getAccounts()->twitch.getCurrent();

    // Avoid Helix calls without Client ID and/or OAuth Token
    if (user->isAnon())
    {
        ctx.channel->addSystemMessage(
            "لازم تسجّل دخول عشان تستخدم /commercial.");
        return "";
    }

    auto broadcasterID = ctx.twitchChannel->roomId();
    auto length = ctx.words.at(1).toInt();

    getHelix()->startCommercial(
        broadcasterID, length,
        [channel{ctx.channel}](auto response) {
            channel->addSystemMessage(
                QString("بدأ إعلان مدته %1 ثانية. تذكّر إنك لسا في البث ومو كل المشاهدين بيشوفون الإعلان. تقدر تشغّل إعلان ثاني بعد %2 ثانية.")
                    .arg(response.length)
                    .arg(response.retryAfter));
        },
        [channel{ctx.channel}](auto error, auto message) {
            auto errorMessage = formatStartCommercialError(error, message);
            channel->addSystemMessage(errorMessage);
        });

    return "";
}

}  // namespace chatterino::commands
