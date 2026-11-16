// SPDX-FileCopyrightText: 2023 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/commands/builtin/twitch/Announce.hpp"

#include "Application.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "controllers/commands/CommandContext.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/twitch/TwitchChannel.hpp"

namespace {
using namespace chatterino;

QString sendAnnouncementColor(const CommandContext &ctx,
                              const HelixAnnouncementColor color)
{
    if (ctx.channel == nullptr)
    {
        return "";
    }

    if (ctx.twitchChannel == nullptr)
    {
        ctx.channel->addSystemMessage(
            "هذا الأمر يشتغل بس في قنوات Twitch.");
        return "";
    }

    QString colorStr = "";
    if (color != HelixAnnouncementColor::Primary)
    {
        colorStr = qmagicenum::enumNameString(color).toLower();
    }

    if (ctx.words.size() < 2)
    {
        QString usageMsg;
        if (color == HelixAnnouncementColor::Primary)
        {
            usageMsg = "الاستخدام: /announce <رسالة> - يبرز رسالتك كإعلان.";
        }
        else
        {
            usageMsg =
                QString("الاستخدام: /announce%1 <رسالة> - يبرز رسالتك كإعلان بلون %1.")
                    .arg(colorStr);
        }
        ctx.channel->addSystemMessage(usageMsg);
        return "";
    }

    auto user = getApp()->getAccounts()->twitch.getCurrent();
    if (user->isAnon())
    {
        ctx.channel->addSystemMessage(
            QString("لازم تسجّل دخول عشان تستخدم /announce%1.")
                .arg(colorStr));
        return "";
    }

    getHelix()->sendChatAnnouncement(
        ctx.twitchChannel->roomId(), user->getUserId(),
        ctx.words.mid(1).join(" "), color,
        []() {
            // do nothing.
        },
        [channel{ctx.channel}](auto error, auto message) {
            using Error = HelixSendChatAnnouncementError;
            QString errorMessage = QString("فشل إرسال الإعلان - ");

            switch (error)
            {
                case Error::UserMissingScope: {
                    // TODO(pajlada): Phrase MISSING_REQUIRED_SCOPE
                    errorMessage +=
                        "صلاحية ناقصة. سجّل دخولك من جديد وحاول مرة ثانية.";
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

            channel->addSystemMessage(errorMessage);
        });
    return "";
}

}  // namespace

namespace chatterino::commands {

QString sendAnnouncement(const CommandContext &ctx)
{
    return sendAnnouncementColor(ctx, HelixAnnouncementColor::Primary);
}

QString sendAnnouncementBlue(const CommandContext &ctx)
{
    return sendAnnouncementColor(ctx, HelixAnnouncementColor::Blue);
}

QString sendAnnouncementGreen(const CommandContext &ctx)
{
    return sendAnnouncementColor(ctx, HelixAnnouncementColor::Green);
}

QString sendAnnouncementOrange(const CommandContext &ctx)
{
    return sendAnnouncementColor(ctx, HelixAnnouncementColor::Orange);
}

QString sendAnnouncementPurple(const CommandContext &ctx)
{
    return sendAnnouncementColor(ctx, HelixAnnouncementColor::Purple);
}

}  // namespace chatterino::commands
