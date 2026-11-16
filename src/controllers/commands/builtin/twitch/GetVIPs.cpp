// SPDX-FileCopyrightText: 2023 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/commands/builtin/twitch/GetVIPs.hpp"

#include "Application.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/commands/CommandContext.hpp"
#include "messages/MessageBuilder.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchChannel.hpp"

namespace {

using namespace chatterino;

QString formatGetVIPsError(HelixListVIPsError error, const QString &message)
{
    using Error = HelixListVIPsError;

    QString errorMessage = QString("فشل عرض الـ VIP - ");

    switch (error)
    {
        case Error::Forwarded: {
            errorMessage += message;
        }
        break;

        case Error::Ratelimited: {
            errorMessage += "Twitch حاط عليك حد مؤقت. حاول بعد ثواني.";
        }
        break;

        case Error::UserMissingScope: {
            // TODO(pajlada): Phrase MISSING_REQUIRED_SCOPE
            errorMessage += "صلاحية ناقصة. سجّل دخولك من جديد وحاول مرة ثانية.";
        }
        break;

        case Error::UserNotAuthorized: {
            // TODO(pajlada): Phrase MISSING_PERMISSION
            errorMessage += "ما عندك صلاحية تسوي هذا الإجراء.";
        }
        break;

        case Error::UserNotBroadcaster: {
            errorMessage +=
                "بسبب قيود Twitch، هذا الأمر لصاحب القناة بس. لعرض الـ VIP استخدم موقع Twitch.";
        }
        break;

        case Error::Unknown: {
            errorMessage += "صار خطأ غير معروف.";
        }
        break;
    }
    return errorMessage;
}

}  // namespace

namespace chatterino::commands {

QString getVIPs(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    if (ctx.twitchChannel == nullptr)
    {
        ctx.channel->addSystemMessage(
            "الأمر /vips يشتغل بس في قنوات Twitch.");
        return "";
    }

    auto currentUser = getApp()->getAccounts()->twitch.getCurrent();
    if (currentUser->isAnon())
    {
        ctx.channel->addSystemMessage(
            "بسبب قيود Twitch، هذا الأمر لصاحب القناة بس. لعرض الـ VIP استخدم موقع Twitch.");
        return "";
    }

    getHelix()->getChannelVIPs(
        ctx.twitchChannel->roomId(),
        [channel{ctx.channel}, twitchChannel{ctx.twitchChannel}](
            const std::vector<HelixVip> &vipList) {
            if (vipList.empty())
            {
                channel->addSystemMessage(
                    "هذي القناة ما فيها VIP.");
                return;
            }

            auto messagePrefix = QString("الـ VIP في هذي القناة هم");

            // TODO: sort results?

            channel->addMessage(MessageBuilder::makeListOfUsersMessage(
                                    messagePrefix, vipList, twitchChannel),
                                MessageContext::Original);
        },
        [channel{ctx.channel}](auto error, auto message) {
            auto errorMessage = formatGetVIPsError(error, message);
            channel->addSystemMessage(errorMessage);
        });

    return "";
}

}  // namespace chatterino::commands
