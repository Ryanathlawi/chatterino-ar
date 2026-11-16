// SPDX-FileCopyrightText: 2023 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/commands/builtin/twitch/GetModerators.hpp"

#include "common/Channel.hpp"
#include "controllers/commands/CommandContext.hpp"
#include "messages/MessageBuilder.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/TwitchChannel.hpp"

namespace {

using namespace chatterino;

QString formatModsError(HelixGetModeratorsError error, const QString &message)
{
    using Error = HelixGetModeratorsError;

    QString errorMessage = QString("فشل جلب المشرفين - ");

    switch (error)
    {
        case Error::Forwarded: {
            errorMessage += message;
        }
        break;

        case Error::UserMissingScope: {
            errorMessage += "صلاحية ناقصة. سجّل دخولك من جديد وحاول مرة ثانية.";
        }
        break;

        case Error::UserNotAuthorized: {
            errorMessage +=
                "بسبب قيود Twitch، هذا الأمر لصاحب القناة بس. لعرض المشرفين استخدم موقع Twitch.";
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

QString getModerators(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    if (ctx.twitchChannel == nullptr)
    {
        ctx.channel->addSystemMessage(
            "الأمر /mods يشتغل بس في قنوات Twitch.");
        return "";
    }

    getHelix()->getModerators(
        ctx.twitchChannel->roomId(), 500,
        [channel{ctx.channel}, twitchChannel{ctx.twitchChannel}](auto result) {
            if (result.empty())
            {
                channel->addSystemMessage(
                    "هذي القناة ما فيها مشرفين.");
                return;
            }

            // TODO: sort results?

            channel->addMessage(MessageBuilder::makeListOfUsersMessage(
                                    "مشرفين هذي القناة هم",
                                    result, twitchChannel),
                                MessageContext::Original);
        },
        [channel{ctx.channel}](auto error, auto message) {
            auto errorMessage = formatModsError(error, message);
            channel->addSystemMessage(errorMessage);
        });

    return "";
}

}  // namespace chatterino::commands
