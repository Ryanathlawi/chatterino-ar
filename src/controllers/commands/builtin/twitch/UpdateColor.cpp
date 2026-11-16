// SPDX-FileCopyrightText: 2023 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/commands/builtin/twitch/UpdateColor.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/commands/CommandContext.hpp"
#include "messages/MessageBuilder.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "util/Twitch.hpp"

namespace chatterino::commands {

QString updateUserColor(const CommandContext &ctx)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    if (!ctx.channel->isTwitchChannel())
    {
        ctx.channel->addSystemMessage(
            "الأمر /color يشتغل بس في قنوات Twitch.");
        return "";
    }
    auto user = getApp()->getAccounts()->twitch.getCurrent();

    // Avoid Helix calls without Client ID and/or OAuth Token
    if (user->isAnon())
    {
        ctx.channel->addSystemMessage(
            "لازم تسجّل دخول عشان تستخدم /color.");
        return "";
    }

    auto colorString = ctx.words.value(1);

    if (colorString.isEmpty())
    {
        ctx.channel->addSystemMessage(
            QString("الاستخدام: /color <اللون> - اللون لازم يكون من ألوان Twitch (%1) أو كود hex (#000000) إذا عندك Turbo أو Prime.")
                .arg(VALID_HELIX_COLORS.join(", ")));
        return "";
    }

    cleanHelixColorName(colorString);

    getHelix()->updateUserChatColor(
        user->getUserId(), colorString,
        [colorString, channel{ctx.channel}] {
            QString successMessage =
                QString("تم تغيير لونك إلى %1.").arg(colorString);
            channel->addSystemMessage(successMessage);
        },
        [colorString, channel{ctx.channel}](auto error, auto message) {
            QString errorMessage =
                QString("فشل تغيير اللون إلى %1 - ").arg(colorString);

            switch (error)
            {
                case HelixUpdateUserChatColorError::UserMissingScope: {
                    errorMessage +=
                        "صلاحية ناقصة. سجّل دخولك من جديد وحاول مرة ثانية.";
                }
                break;

                case HelixUpdateUserChatColorError::InvalidColor: {
                    errorMessage += QString("اللون لازم يكون من ألوان Twitch (%1) أو كود hex (#000000) إذا عندك Turbo أو Prime.")
                                        .arg(VALID_HELIX_COLORS.join(", "));
                }
                break;

                case HelixUpdateUserChatColorError::Forwarded: {
                    errorMessage += message + ".";
                }
                break;

                case HelixUpdateUserChatColorError::Unknown:
                default: {
                    errorMessage += "صار خطأ غير معروف.";
                }
                break;
            }

            channel->addSystemMessage(errorMessage);
        });

    return "";
}

}  // namespace chatterino::commands
